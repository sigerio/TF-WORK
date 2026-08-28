#include "tf_modbus_master.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "com_link.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"
#include "tf_modbus_txq.h"
#include "tf_modbus_ac.h"
#include "tf_modbus_xm7828.h"
#include "tf_modbus_xm8189.h"
#include "tf_modbus_xm8537.h"
#include "tf_modbus_xm9505.h"
#include "../../LIB/m_bit.h"
#include "tf_debug.h"

/*
 * ─────────────────────────────────────────────────────────────────────
 * tf_modbus_master — Modbus RTU 多总线主站模块
 * ─────────────────────────────────────────────────────────────────────
 *
 *  层次结构
 * ─────────────────────────────────────────────────────────────────────
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  1. 收发层 (com_link，每条总线一个实例)                          │
 *  │     TF_MODBUS_BUS_WEATHER → USART0 + DMA（方向脚 PA8）           │
 *  │     TF_MODBUS_BUS_AC      → UART3  + DMA（方向脚 PA15）          │
 *  │     每条总线独立持有 rx_dma_buf / rx_fifo / tx_dma_buf，          │
 *  │     通过 com_link_resource_t 注册进公共收发层                     │
 *  │                                                                 │
 *  │  2. 主站层 (master)                                             │
 *  │     每条总线独立的 状态机(IDLE/WAIT_RSP) + rsp_expect + 超时      │
 *  │                                                                 │
 *  │  3. 设备层 (cache)                                              │
 *  │     xm7828/xm8189/xm8537/xm9505（气象板总线） + ac（空调总线）    │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  数据流（发送）：
 *    tf_modbus_query_task() → 设备 read 接口组帧（expect.bus 标识总线）
 *      → tf_modbus_txq_push() 入统一发送队列
 *      → master_task() 检测某总线空闲 → tf_modbus_txq_pop(bus)
 *      → 拷贝到该总线 com_link 的 tx_dma_buf → com_link_tx_start()
 *
 *  数据流（接收）：
 *    USART IDLE 中断 → com_link_irq() → dma_buf 压入该总线 rx_fifo
 *      → master_task() 在该总线上按 rsp_expect 匹配帧 → 解析 → 更新 cache
 * ─────────────────────────────────────────────────────────────────────
 */

#ifndef MODULE_MODBUS_RESPONSE_TIMEOUT_MS
#define MODULE_MODBUS_RESPONSE_TIMEOUT_MS      60U
#endif

#ifndef MODULE_MODBUS_XM7828_ADDR
#define MODULE_MODBUS_XM7828_ADDR              5U
#endif

#ifndef MODULE_MODBUS_XM8189_ADDR
#define MODULE_MODBUS_XM8189_ADDR              2U
#endif

#ifndef MODULE_MODBUS_XM8537_ADDR
#define MODULE_MODBUS_XM8537_ADDR              3U
#endif

#ifndef MODULE_MODBUS_XM9505_ADDR
#define MODULE_MODBUS_XM9505_ADDR              4U
#endif

#ifndef MODULE_MODBUS_AC_ADDR
#define MODULE_MODBUS_AC_ADDR                   1U
#endif

typedef enum {
    TF_MODBUS_QUERY_XM7828 = 0,
    TF_MODBUS_QUERY_XM8189,
    TF_MODBUS_QUERY_XM8537,
    TF_MODBUS_QUERY_XM9505,
    TF_MODBUS_QUERY_XM7828_CALIBRATION,
    TF_MODBUS_QUERY_XM8189_CALIBRATION,
    TF_MODBUS_QUERY_AC_MEASURE,
    TF_MODBUS_QUERY_AC_HUMIDITY,
    TF_MODBUS_QUERY_AC_CONTROL,
    TF_MODBUS_QUERY_AC_STATUS,
    TF_MODBUS_QUERY_STEP_COUNT,
} tf_modbus_query_step_t;

/* 主站状态机：IDLE →（出队发帧）→ WAIT_RSP →（收到/超时）→ IDLE */
typedef enum {
    TF_MODBUS_MASTER_IDLE = 0,      /**< 空闲，可发送下一帧                  */
    TF_MODBUS_MASTER_WAIT_RSP,      /**< 等待从机响应中                      */
} tf_modbus_master_state_t;

/** @brief 每条总线的主站运行时上下文 */
typedef struct {
#if MODULE_MODBUS_ENABLE_DIAG
    tf_modbus_diag_t diag;
#endif
    tf_modbus_master_state_t state;
    tf_modbus_rsp_expect_t rsp_expect;
    uint32_t wait_start_tick;
    uint32_t task_tick;
} tf_modbus_master_rt_t;

/** @brief 设备数据缓存（所有从机的上次成功解析数据） */
typedef struct {
    xm7828_th_data_t xm7828;
    xm8189_wind_data_t xm8189;
    xm8537_water_data_t xm8537;
    xm9505_rain_data_t xm9505;
    ac_device_t ac;
} tf_modbus_data_cache_t;

/** @brief 单条总线的全部资源：com_link 句柄 + 模块自有缓冲 + 主站状态 */
typedef struct {
    com_link_t link;                                     /**< 公共收发层句柄 */
    com_link_resource_t res;                             /**< 注册资源表     */
    uint8_t rx_dma_buf[MODULE_MODBUS_RX_BUF_SIZE];
    uint8_t rx_fifo_buf[MODULE_MODBUS_RINGBUF_SIZE];
    ringbuf_t rx_fifo;
    uint8_t tx_dma_buf[MODULE_MODBUS_TX_BUF_SIZE];
    tf_modbus_master_rt_t master;                        /**< 主站状态机     */
} tf_modbus_bus_ctx_t;

typedef bool (*tf_modbus_query_fn_t)(uint8_t slave_addr,
                                     uint16_t start_reg,
                                     uint16_t reg_count);

typedef struct {
    tf_modbus_query_fn_t read;
    const uint8_t *slave_addr;
    uint16_t start_addr;
    uint16_t quantity;
} tf_modbus_query_plan_t;

/* ── 各总线静态实例（每条总线一套独立缓冲与状态） ── */
static tf_modbus_bus_ctx_t s_bus[TF_MODBUS_BUS_COUNT];

/* ── 设备数据缓存 ── */
static tf_modbus_data_cache_t s_cache = {
    .xm7828 = { .slave_addr = MODULE_MODBUS_XM7828_ADDR },
    .xm8189 = { .slave_addr = MODULE_MODBUS_XM8189_ADDR },
    .xm8537 = { .slave_addr = MODULE_MODBUS_XM8537_ADDR },
    .xm9505 = { .slave_addr = MODULE_MODBUS_XM9505_ADDR },
    .ac = { .slave_addr = MODULE_MODBUS_AC_ADDR },
};

static const char *tf_modbus_bus_name(tf_modbus_bus_t bus)
{
    return (bus == TF_MODBUS_BUS_WEATHER) ? "WEATHER" : "AC";
}

static void tf_modbus_log_frame(const char *direction,
                                tf_modbus_bus_t bus,
                                const uint8_t *frame,
                                uint32_t length)
{
    if(0 == (debug_ctrl.bits.ac&&debug_ctrl.bits.weth)) return;
    char line[128];
    uint32_t offset = 0U;

    /* 本机日志只监控 AC 空调；气象板通信保持静默。 */
    if ((bus != TF_MODBUS_BUS_AC) || (direction == NULL) ||
        (frame == NULL) || (length == 0U)) {
        return;
    }

    /* 分段打印，避免长 Modbus 响应超过调试串口格式化缓冲区。 */
    while (offset < length) {
        int used = snprintf(line, sizeof(line),
                            "MODBUS %s bus=%s len=%lu:", direction,
                            tf_modbus_bus_name(bus), (unsigned long)length);
        uint32_t count = 0U;

        if (used < 0) {
            return;
        }
        while ((count < 24U) && (offset < length) &&
               ((uint32_t)used + 4U < sizeof(line))) {
            used += snprintf(&line[used], sizeof(line) - (size_t)used,
                             " %02X", frame[offset++]);
            count++;
        }
        (void)snprintf(&line[used], sizeof(line) - (size_t)used, "\n");
        tf_debug_printf(TF_COLOR_RESET "%s[%-6s]" TF_COLOR_RESET " %s",
                        tf_tag_color("APP"), "APP", line);
    }
}

static uint32_t s_query_step;

static const tf_modbus_query_plan_t periodic_query_plan[TF_MODBUS_QUERY_STEP_COUNT] = {
    [TF_MODBUS_QUERY_XM7828] = {
        tf_modbus_xm7828_read, &s_cache.xm7828.slave_addr,
        XUNCHIP_REG_DATA0, XM7828_MEASUREMENT_REG_COUNT,
    },
    [TF_MODBUS_QUERY_XM8189] = {
        tf_modbus_xm8189_read, &s_cache.xm8189.slave_addr,
        XUNCHIP_REG_DATA0, XM8189_MEASUREMENT_REG_COUNT,
    },
    [TF_MODBUS_QUERY_XM8537] = {
        tf_modbus_xm8537_read, &s_cache.xm8537.slave_addr,
        XUNCHIP_REG_DATA0, XM8537_MEASUREMENT_REG_COUNT,
    },
    [TF_MODBUS_QUERY_XM9505] = {
        tf_modbus_xm9505_read, &s_cache.xm9505.slave_addr,
        XUNCHIP_REG_DATA0, XM9505_MEASUREMENT_REG_COUNT,
    },
    [TF_MODBUS_QUERY_XM7828_CALIBRATION] = {
        tf_modbus_xm7828_read, &s_cache.xm7828.slave_addr,
        XM7828_REG_TEMP_CALIBRATION, XM7828_CALIBRATION_REG_COUNT,
    },
    [TF_MODBUS_QUERY_XM8189_CALIBRATION] = {
        tf_modbus_xm8189_read, &s_cache.xm8189.slave_addr,
        XM8189_REG_SPEED_CALIBRATION, XM8189_CALIBRATION_REG_COUNT,
    },
    [TF_MODBUS_QUERY_AC_MEASURE] = {
        tf_modbus_ac_read_params, &s_cache.ac.slave_addr,
        AC_PARAM_SEG_MEASURE_START, AC_PARAM_SEG_MEASURE_COUNT,
    },
    [TF_MODBUS_QUERY_AC_HUMIDITY] = {
        tf_modbus_ac_read_params, &s_cache.ac.slave_addr,
        AC_PARAM_SEG_HUMIDITY_START, AC_PARAM_SEG_HUMIDITY_COUNT,
    },
    [TF_MODBUS_QUERY_AC_CONTROL] = {
        tf_modbus_ac_read_params, &s_cache.ac.slave_addr,
        AC_PARAM_SEG_CONTROL_START, AC_PARAM_SEG_CONTROL_COUNT,
    },
    [TF_MODBUS_QUERY_AC_STATUS] = {
        tf_modbus_ac_read_status, &s_cache.ac.slave_addr,
        AC_STATUS_SEG_ALL_START, AC_STATUS_SEG_ALL_COUNT,
    },
};

#if MODULE_MODBUS_ENABLE_DIAG
#define TF_MODBUS_DIAG_INC(bus_, field_) (s_bus[bus_].master.diag.field_++)
#else
#define TF_MODBUS_DIAG_INC(bus_, field_) ((void)0)
#endif

static void tf_modbus_bus_resource_init(tf_modbus_bus_t bus,
                                        board_usart_id_e usart_id)
{
    tf_modbus_bus_ctx_t *ctx = &s_bus[bus];

    ctx->res.usart = &board_usart_configs[usart_id];
    ctx->res.rx_dma_buf = ctx->rx_dma_buf;
    ctx->res.rx_dma_size = (uint16_t)sizeof(ctx->rx_dma_buf);
    ctx->res.rx_fifo = &ctx->rx_fifo;
    ctx->res.tx_dma_buf = ctx->tx_dma_buf;
    ctx->res.tx_dma_size = (uint16_t)sizeof(ctx->tx_dma_buf);
}

static void tf_modbus_reset_devices(void)
{
    memset(&s_cache.xm7828.status, 0, sizeof(s_cache.xm7828.status));
    memset(&s_cache.xm8189.status, 0, sizeof(s_cache.xm8189.status));
    memset(&s_cache.xm8537.status, 0, sizeof(s_cache.xm8537.status));
    memset(&s_cache.xm9505.status, 0, sizeof(s_cache.xm9505.status));
    memset(&s_cache.ac.status, 0, sizeof(s_cache.ac.status));
    cancel_signal(OS_485_TH_ONLINE);
    cancel_signal(OS_485_WIND_ONLINE);
    cancel_signal(OS_485_WATER_ONLINE);
    cancel_signal(OS_485_RAIN_ONLINE);
    cancel_signal(OS_485_AC_ONLINE);
}

static void tf_modbus_reset_runtime(void)
{
    tf_modbus_txq_reset();
    for (uint32_t bus = 0U; bus < (uint32_t)TF_MODBUS_BUS_COUNT; bus++) {
        tf_modbus_master_rt_t *master = &s_bus[bus].master;

        com_link_reset(&s_bus[bus].link);
#if MODULE_MODBUS_ENABLE_DIAG
        memset(&master->diag, 0, sizeof(master->diag));
#endif
        memset(&master->rsp_expect, 0, sizeof(master->rsp_expect));
        master->state = TF_MODBUS_MASTER_IDLE;
        master->wait_start_tick = 0U;
        master->task_tick = 0U;
    }
    s_query_step = 0U;
    tf_modbus_reset_devices();
}

static void tf_modbus_wait_start(tf_modbus_bus_t bus)
{
    tf_modbus_master_rt_t *master = &s_bus[bus].master;

    master->state = TF_MODBUS_MASTER_WAIT_RSP;
    master->wait_start_tick = master->task_tick;
    TF_MODBUS_DIAG_INC(bus, request_count);
}

static void tf_modbus_wait_done(tf_modbus_bus_t bus)
{
    s_bus[bus].master.state = TF_MODBUS_MASTER_IDLE;
}

static void tf_modbus_ac_update_fault(void)
{
    ac_status_t *status = &s_cache.ac.run_status;

    s_cache.ac.status.sensor_fault = (status->cabinet_sensor_fault ||
                                      status->evaporator_sensor_fault ||
                                      status->condenser_sensor_fault ||
                                      status->humidity_sensor_fault);
}

static tf_modbus_dev_status_t *tf_modbus_status_of(tf_modbus_dev_type_t dev_type)
{
    switch (dev_type) {
    case TF_MODBUS_DEV_XM7828_TH:   return &s_cache.xm7828.status;
    case TF_MODBUS_DEV_XM8189_WIND: return &s_cache.xm8189.status;
    case TF_MODBUS_DEV_XM8537_WATER:return &s_cache.xm8537.status;
    case TF_MODBUS_DEV_XM9505_RAIN: return &s_cache.xm9505.status;
    case TF_MODBUS_DEV_AC:          return &s_cache.ac.status;
    default:                        return NULL;
    }
}

static bool tf_modbus_signal_of(tf_modbus_dev_type_t dev_type,
                                OS_RUN_SIGNAL_EN *signal)
{
    if (signal == NULL) {
        return false;
    }

    switch (dev_type) {
    case TF_MODBUS_DEV_XM7828_TH:
        *signal = OS_485_TH_ONLINE;
        return true;

    case TF_MODBUS_DEV_XM8189_WIND:
        *signal = OS_485_WIND_ONLINE;
        return true;

    case TF_MODBUS_DEV_XM8537_WATER:
        *signal = OS_485_WATER_ONLINE;
        return true;

    case TF_MODBUS_DEV_XM9505_RAIN:
        *signal = OS_485_RAIN_ONLINE;
        return true;

    case TF_MODBUS_DEV_AC:
        *signal = OS_485_AC_ONLINE;
        return true;

    default:
        return false;
    }
}

static void tf_modbus_device_offline(const tf_modbus_rsp_expect_t *expect)
{
    tf_modbus_dev_status_t *status;
    OS_RUN_SIGNAL_EN signal;

    if (expect == NULL) {
        return;
    }

    status = tf_modbus_status_of(expect->dev_type);
    if (status != NULL) {
        status->online = false;
        status->sensor_fault = false;
    }
    if (tf_modbus_signal_of(expect->dev_type, &signal)) {
        cancel_signal(signal);
    }
}

static void tf_modbus_device_online(const tf_modbus_rsp_expect_t *expect)
{
    tf_modbus_dev_status_t *status;
    OS_RUN_SIGNAL_EN signal;

    if (expect == NULL) {
        return;
    }

    status = tf_modbus_status_of(expect->dev_type);
    if (status != NULL) {
        status->online = true;
    }
    if (tf_modbus_signal_of(expect->dev_type, &signal)) {
        set_signal(signal);
    }
}

static bool tf_modbus_req_is_read(tf_modbus_req_type_t request_type)
{
    return (request_type == TF_MODBUS_REQ_READ_REGS) ||
           (request_type == TF_MODBUS_REQ_READ_BITS);
}

static bool tf_modbus_req_is_write(tf_modbus_req_type_t request_type)
{
    return (request_type == TF_MODBUS_REQ_WRITE_COIL) ||
           (request_type == TF_MODBUS_REQ_WRITE_REG);
}

/* 谁用谁初始化：单条 485 总线硬件初始化（官方库函数，含方向脚） */
static void tf_modbus_bus_hardware_init(const board_usart_config_t *usart)
{
    const board_gpio_config_t *tx_pin = &board_gpio_configs[usart->tx_pin];
    const board_gpio_config_t *rx_pin = &board_gpio_configs[usart->rx_pin];
    const board_dma_config_t *dma = &board_dma_configs[usart->rx_dma.dma];
    const board_dma_channel_config_t *rx_dma = &dma->ch[usart->rx_dma.ch];
    dma_parameter_struct dma_parameters;

    /* TX/RX/方向脚 GPIO */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_init(tx_pin->port, tx_pin->mode, tx_pin->speed, tx_pin->pin);
    gpio_bit_write(tx_pin->port, tx_pin->pin, tx_pin->initial_level ? SET : RESET);
    gpio_init(rx_pin->port, rx_pin->mode, rx_pin->speed, rx_pin->pin);

    if (usart->direction_pin != BOARD_PIN_NONE) {
        const board_gpio_config_t *dir = &board_gpio_configs[usart->direction_pin];

        gpio_init(dir->port, dir->mode, dir->speed, dir->pin);
        gpio_bit_write(dir->port, dir->pin, dir->initial_level ? SET : RESET);
    }

    /* USART */
    rcu_periph_clock_enable(usart->clock);
    usart_deinit(usart->peripheral);
    usart_baudrate_set(usart->peripheral, usart->baudrate);
    usart_word_length_set(usart->peripheral, USART_WL_8BIT);
    usart_stop_bit_set(usart->peripheral, USART_STB_1BIT);
    usart_parity_config(usart->peripheral, USART_PM_NONE);
    usart_hardware_flow_rts_config(usart->peripheral, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(usart->peripheral, USART_CTS_DISABLE);
    usart_receive_config(usart->peripheral, USART_RECEIVE_ENABLE);
    usart_transmit_config(usart->peripheral, USART_TRANSMIT_ENABLE);
    usart_enable(usart->peripheral);

    /* RX DMA */
    rcu_periph_clock_enable(dma->clock);
    dma_struct_para_init(&dma_parameters);
    dma_parameters.memory_addr = 0U;
    dma_parameters.number = 0U;
    dma_parameters.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_parameters.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_parameters.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_parameters.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_parameters.priority = DMA_PRIORITY_ULTRA_HIGH;

    dma_parameters.periph_addr = rx_dma->peripheral_address;
    dma_parameters.direction = rx_dma->direction;
    dma_deinit(dma->controller, rx_dma->channel);
    dma_init(dma->controller, rx_dma->channel, &dma_parameters);
    dma_circulation_disable(dma->controller, rx_dma->channel);
    dma_memory_to_memory_disable(dma->controller, rx_dma->channel);
    dma_flag_clear(dma->controller, rx_dma->channel,
                   DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);

    /* TX DMA（可选：无 DMA 通道时跳过，TX 走轮询） */
    if (BOARD_DMA_HAS(usart->tx_dma)) {
        const board_dma_config_t *tx_dma_ctl = &board_dma_configs[usart->tx_dma.dma];
        const board_dma_channel_config_t *tx_dma = &tx_dma_ctl->ch[usart->tx_dma.ch];

        rcu_periph_clock_enable(tx_dma_ctl->clock);
        dma_parameters.periph_addr = tx_dma->peripheral_address;
        dma_parameters.direction = tx_dma->direction;
        dma_deinit(tx_dma_ctl->controller, tx_dma->channel);
        dma_init(tx_dma_ctl->controller, tx_dma->channel, &dma_parameters);
        dma_circulation_disable(tx_dma_ctl->controller, tx_dma->channel);
        dma_memory_to_memory_disable(tx_dma_ctl->controller, tx_dma->channel);
        dma_flag_clear(tx_dma_ctl->controller, tx_dma->channel,
                       DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
        usart_dma_transmit_config(usart->peripheral, USART_TRANSMIT_DMA_ENABLE);
    }

    usart_dma_receive_config(usart->peripheral, USART_RECEIVE_DMA_ENABLE);
}

bool tf_modbus_master_init(void)
{
    tf_modbus_bus_resource_init(TF_MODBUS_BUS_WEATHER, BOARD_USART_WEATHER);
    tf_modbus_bus_resource_init(TF_MODBUS_BUS_AC, BOARD_USART_AC);

    for (uint32_t bus = 0U; bus < (uint32_t)TF_MODBUS_BUS_COUNT; bus++) {
        tf_modbus_bus_ctx_t *ctx = &s_bus[bus];

        /* 谁用谁初始化：本总线硬件 + NVIC 由本模块负责（官方库函数） */
        tf_modbus_bus_hardware_init(ctx->res.usart);
        nvic_irq_enable(com_link_usart_irqn(ctx->res.usart->peripheral), 1U, 0U);

        ringbuf_init(&ctx->rx_fifo, ctx->rx_fifo_buf, sizeof(ctx->rx_fifo_buf));
        if (!com_link_open(&ctx->link, &ctx->res)) {
            return false;
        }
    }

    tf_modbus_reset_runtime();
    return true;
}

#if MODULE_MODBUS_ENABLE_DIAG
void tf_modbus_get_diag(tf_modbus_diag_t *diag)
{
    tf_modbus_txq_diag_t queue_diag;

    if (diag == NULL) {
        return;
    }

    memset(diag, 0, sizeof(*diag));
    for (uint32_t bus = 0U; bus < (uint32_t)TF_MODBUS_BUS_COUNT; bus++) {
        diag->task_tick += s_bus[bus].master.task_tick;
        diag->request_count += s_bus[bus].master.diag.request_count;
        diag->response_count += s_bus[bus].master.diag.response_count;
        diag->timeout_count += s_bus[bus].master.diag.timeout_count;
        diag->crc_error_count += s_bus[bus].master.diag.crc_error_count;
        diag->frame_error_count += s_bus[bus].master.diag.frame_error_count;
        diag->unmatched_count += s_bus[bus].master.diag.unmatched_count;
        diag->parse_ok_count += s_bus[bus].master.diag.parse_ok_count;
        diag->parse_error_count += s_bus[bus].master.diag.parse_error_count;
    }

    tf_modbus_txq_get_diag(&queue_diag);
    diag->txq_write_pos = queue_diag.write_pos;
    diag->enqueue_count = queue_diag.enqueue_count;
    diag->dequeue_count = queue_diag.dequeue_count;
    diag->txq_overwrite_drop_count = queue_diag.overwrite_drop_count;
    diag->txq_full_drop_count = queue_diag.full_drop_count;
    diag->txq_invalid_count = queue_diag.invalid_count;
}

void tf_modbus_clear_diag(void)
{
    for (uint32_t bus = 0U; bus < (uint32_t)TF_MODBUS_BUS_COUNT; bus++) {
        memset(&s_bus[bus].master.diag, 0, sizeof(s_bus[bus].master.diag));
    }
    tf_modbus_txq_clear_diag();
}
#endif

static bool tf_modbus_copy_cache(void *out, const void *cache, uint32_t size)
{
    if ((out == NULL) || (cache == NULL) || (size == 0U)) {
        return false;
    }

    memcpy(out, cache, size);
    return true;
}

bool tf_modbus_xm7828_get(xm7828_th_data_t *out)
{
    return tf_modbus_copy_cache(out, &s_cache.xm7828, sizeof(*out));
}

bool tf_modbus_xm8189_get(xm8189_wind_data_t *out)
{
    return tf_modbus_copy_cache(out, &s_cache.xm8189, sizeof(*out));
}

bool tf_modbus_xm8537_get(xm8537_water_data_t *out)
{
    return tf_modbus_copy_cache(out, &s_cache.xm8537, sizeof(*out));
}

bool tf_modbus_xm9505_get(xm9505_rain_data_t *out)
{
    return tf_modbus_copy_cache(out, &s_cache.xm9505, sizeof(*out));
}

bool tf_modbus_ac_get_params(ac_params_t *out)
{
    ac_params_t *cache = &s_cache.ac.params;

    if (!tf_modbus_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return (cache->marks.raw != 0U);
}

bool tf_modbus_ac_get_status(ac_status_t *out)
{
    ac_status_t *cache = &s_cache.ac.run_status;

    if (!tf_modbus_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return (cache->marks.raw != 0U);
}

bool tf_modbus_ac_get(ac_device_t *out)
{
    ac_device_t *cache = &s_cache.ac;

    if (!tf_modbus_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return cache->status.online;
}

static bool tf_modbus_wait_pop_frame(tf_modbus_bus_t bus,
                                     uint8_t *frame,
                                     uint32_t *frame_len)
{
    tf_modbus_master_rt_t *master = &s_bus[bus].master;
    tf_modbus_rsp_expect_t *expect = &master->rsp_expect;
    com_link_t *link = &s_bus[bus].link;
    uint8_t head[3];
    uint8_t expect_function;
    uint8_t expect_byte_count;
    uint32_t available;
    uint32_t expected_len;
    uint32_t scan_budget;
    bool exception_response;
    bool read_request;
    bool write_request;

    if ((frame == NULL) || (frame_len == NULL) ||
        (master->state != TF_MODBUS_MASTER_WAIT_RSP)) {
        return false;
    }

    read_request = tf_modbus_req_is_read(expect->req_type);
    write_request = tf_modbus_req_is_write(expect->req_type);

    if ((expect->slave_addr == 0U) || (!read_request && !write_request)) {
        tf_modbus_device_offline(expect);
        TF_MODBUS_DIAG_INC(bus, frame_error_count);
        tf_modbus_wait_done(bus);
        return false;
    }

    expect_function = expect->rsp_func;
    expect_byte_count = expect->rsp_data_bytes;
    if (read_request) {
        if ((expect_byte_count == 0U) ||
            (((uint32_t)expect_byte_count + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
            tf_modbus_device_offline(expect);
            TF_MODBUS_DIAG_INC(bus, frame_error_count);
            tf_modbus_wait_done(bus);
            return false;
        }
        expected_len = (uint32_t)expect_byte_count + 5U;
    } else {
        expected_len = MODBUS_RTU_WRITE_SINGLE_RESPONSE_LEN;
    }
    scan_budget = com_link_rx_available(link);

    while (scan_budget >= 2U) {
        available = com_link_rx_available(link);
        if (available < 2U) {
            return false;
        }

        if (com_link_rx_peek(link, head, 2U) != 2U) {
            return false;
        }

        exception_response = (head[1] == (uint8_t)(expect_function | 0x80U));
        if ((head[0] != expect->slave_addr) ||
            ((head[1] != expect_function) && !exception_response)) {
            if (com_link_rx_drop(link, 1U) != 1U) {
                return false;
            }
            TF_MODBUS_DIAG_INC(bus, unmatched_count);
            scan_budget--;
            continue;
        }

        if (exception_response) {
            expected_len = MODBUS_RTU_EXCEPTION_LEN;
            available = com_link_rx_available(link);
            if (available < expected_len) {
                return false;
            }

            if (com_link_rx_peek(link, frame, (uint16_t)expected_len) != expected_len) {
                return false;
            }

            (void)com_link_rx_drop(link, (uint16_t)expected_len);
            *frame_len = expected_len;
            return true;
        }

        if (read_request) {
            available = com_link_rx_available(link);
            if (available < 3U) {
                return false;
            }

            if (com_link_rx_peek(link, head, 3U) != 3U) {
                return false;
            }

            if (head[2] != expect_byte_count) {
                (void)com_link_rx_drop(link, 3U);
                tf_modbus_device_offline(expect);
                TF_MODBUS_DIAG_INC(bus, frame_error_count);
                tf_modbus_wait_done(bus);
                return false;
            }
        }

        available = com_link_rx_available(link);
        if (available < expected_len) {
            return false;
        }

        if (com_link_rx_peek(link, frame, (uint16_t)expected_len) != expected_len) {
            return false;
        }

        (void)com_link_rx_drop(link, (uint16_t)expected_len);
        *frame_len = expected_len;
        return true;
    }

    return false;
}

typedef modbus_rtu_status_t (*tf_modbus_rsp_parse_fn_t)(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache);

static modbus_rtu_status_t tf_modbus_parse_wait_xm_read(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case TF_MODBUS_DEV_XM7828_TH:
        if ((expect->start_addr == XM7828_REG_TEMP_CALIBRATION) &&
            (expect->quantity == XM7828_CALIBRATION_REG_COUNT)) {
            return tf_modbus_xm7828_parse_calibration(
                frame, len, expect->slave_addr, &cache->xm7828);
        }
        return tf_modbus_xm7828_parse(frame, len, expect->slave_addr,
                                      &cache->xm7828);
    case TF_MODBUS_DEV_XM8189_WIND:
        if ((expect->start_addr == XM8189_REG_SPEED_CALIBRATION) &&
            (expect->quantity == XM8189_CALIBRATION_REG_COUNT)) {
            return tf_modbus_xm8189_parse_calibration(
                frame, len, expect->slave_addr, &cache->xm8189);
        }
        return tf_modbus_xm8189_parse(frame, len, expect->slave_addr,
                                      &cache->xm8189);
    case TF_MODBUS_DEV_XM8537_WATER:
        return tf_modbus_xm8537_parse(frame, len, expect->slave_addr,
                                      &cache->xm8537);
    case TF_MODBUS_DEV_XM9505_RAIN:
        return tf_modbus_xm9505_parse(frame, len, expect->slave_addr,
                                      &cache->xm9505);
    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t tf_modbus_parse_wait_xm_write(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case TF_MODBUS_DEV_XM7828_TH:
        return tf_modbus_xm7828_parse_write_ack(frame, len, expect->slave_addr,
                                                expect->rsp_func,
                                                expect->echo_addr,
                                                expect->echo_value,
                                                &cache->xm7828);
    case TF_MODBUS_DEV_XM8189_WIND:
        return tf_modbus_xm8189_parse_write_ack(frame, len, expect->slave_addr,
                                                expect->rsp_func,
                                                expect->echo_addr,
                                                expect->echo_value,
                                                &cache->xm8189);
    case TF_MODBUS_DEV_XM8537_WATER:
        return tf_modbus_xm8537_parse_write_ack(frame, len, expect->slave_addr,
                                                expect->rsp_func,
                                                expect->echo_addr,
                                                expect->echo_value,
                                                &cache->xm8537);
    case TF_MODBUS_DEV_XM9505_RAIN:
        return tf_modbus_xm9505_parse_write_ack(frame, len, expect->slave_addr,
                                                expect->rsp_func,
                                                expect->echo_addr,
                                                expect->echo_value,
                                                &cache->xm9505);
    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t tf_modbus_parse_wait_ac_regs(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache)
{
    return tf_modbus_ac_parse_params(frame, len, expect->slave_addr,
                                     expect->start_addr, expect->quantity,
                                     &cache->ac.params);
}

static modbus_rtu_status_t tf_modbus_parse_wait_ac_bits(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache)
{
    return tf_modbus_ac_parse_status(frame, len, expect->slave_addr,
                                     expect->start_addr, &cache->ac.run_status);
}

static modbus_rtu_status_t tf_modbus_parse_wait_ac_write(
    const uint8_t *frame,
    uint32_t len,
    const tf_modbus_rsp_expect_t *expect,
    tf_modbus_data_cache_t *cache)
{
    return tf_modbus_ac_parse_write_ack(frame, len, expect->slave_addr,
                                        expect->rsp_func, expect->echo_addr,
                                        expect->echo_value, &cache->ac);
}

static const tf_modbus_rsp_parse_fn_t
    s_rsp_parse_table[TF_MODBUS_DEV_COUNT][TF_MODBUS_REQ_COUNT] = {
    [TF_MODBUS_DEV_XM7828_TH] = {
        [TF_MODBUS_REQ_READ_REGS] = tf_modbus_parse_wait_xm_read,
        [TF_MODBUS_REQ_WRITE_REG] = tf_modbus_parse_wait_xm_write,
    },
    [TF_MODBUS_DEV_XM8189_WIND] = {
        [TF_MODBUS_REQ_READ_REGS] = tf_modbus_parse_wait_xm_read,
        [TF_MODBUS_REQ_WRITE_REG] = tf_modbus_parse_wait_xm_write,
    },
    [TF_MODBUS_DEV_XM8537_WATER] = {
        [TF_MODBUS_REQ_READ_REGS] = tf_modbus_parse_wait_xm_read,
        [TF_MODBUS_REQ_WRITE_REG] = tf_modbus_parse_wait_xm_write,
    },
    [TF_MODBUS_DEV_XM9505_RAIN] = {
        [TF_MODBUS_REQ_READ_REGS] = tf_modbus_parse_wait_xm_read,
        [TF_MODBUS_REQ_WRITE_REG] = tf_modbus_parse_wait_xm_write,
    },
    [TF_MODBUS_DEV_AC] = {
        [TF_MODBUS_REQ_READ_REGS] = tf_modbus_parse_wait_ac_regs,
        [TF_MODBUS_REQ_READ_BITS] = tf_modbus_parse_wait_ac_bits,
        [TF_MODBUS_REQ_WRITE_COIL] = tf_modbus_parse_wait_ac_write,
        [TF_MODBUS_REQ_WRITE_REG] = tf_modbus_parse_wait_ac_write,
    },
};

static modbus_rtu_status_t tf_modbus_parse_waiting(
    tf_modbus_bus_t bus, const uint8_t *frame, uint32_t len)
{
    tf_modbus_rsp_expect_t *expect = &s_bus[bus].master.rsp_expect;
    tf_modbus_rsp_parse_fn_t parse;

    if (((uint32_t)expect->dev_type >= (uint32_t)TF_MODBUS_DEV_COUNT) ||
        ((uint32_t)expect->req_type >= (uint32_t)TF_MODBUS_REQ_COUNT)) {
        return MODBUS_RTU_ERR_FUNC;
    }

    parse = s_rsp_parse_table[expect->dev_type][expect->req_type];
    if (parse == NULL) {
        return MODBUS_RTU_ERR_FUNC;
    }

    return parse(frame, len, expect, &s_cache);
}

static void tf_modbus_count_parse_error(tf_modbus_bus_t bus,
                                        modbus_rtu_status_t status)
{
    if (status == MODBUS_RTU_ERR_CRC) {
        TF_MODBUS_DIAG_INC(bus, crc_error_count);
    } else {
        TF_MODBUS_DIAG_INC(bus, parse_error_count);
    }
}

static void tf_modbus_process_frame(tf_modbus_bus_t bus,
                                    const uint8_t *frame, uint32_t len)
{
    tf_modbus_master_rt_t *master = &s_bus[bus].master;
    tf_modbus_rsp_expect_t *expect = &master->rsp_expect;
    uint8_t expect_function;
    modbus_rtu_status_t status;

    tf_modbus_log_frame("RX", bus, frame, len);
    expect_function = expect->rsp_func;
    TF_MODBUS_DIAG_INC(bus, response_count);
    if (frame[1] == (uint8_t)(expect_function | 0x80U)) {
        modbus_rtu_exception_t exception;

        status = modbus_rtu_parse_exception(frame, len, expect->slave_addr,
                                            expect_function, &exception);

        tf_modbus_count_parse_error(bus, status);
        if ((status == MODBUS_RTU_OK) &&
            tf_modbus_req_is_write(expect->req_type)) {
            tf_modbus_device_online(expect);
        } else {
            tf_modbus_device_offline(expect);
        }
        tf_modbus_wait_done(bus);
        return;
    }

    status = tf_modbus_parse_waiting(bus, frame, len);
    if (status == MODBUS_RTU_OK) {
        tf_modbus_device_online(expect);
        if (expect->dev_type == TF_MODBUS_DEV_AC) {
            if (expect->req_type == TF_MODBUS_REQ_READ_BITS) {
                tf_modbus_ac_update_fault();
            }
        }
        TF_MODBUS_DIAG_INC(bus, parse_ok_count);
    } else {
        tf_modbus_device_offline(expect);
        tf_modbus_count_parse_error(bus, status);
    }

    tf_modbus_wait_done(bus);
}

static void tf_modbus_parse_rx_frames(tf_modbus_bus_t bus)
{
    uint8_t frame[MODULE_MODBUS_RX_BUF_SIZE];
    uint32_t len = 0U;

    if (s_bus[bus].master.state != TF_MODBUS_MASTER_WAIT_RSP) {
        return;
    }

    if (tf_modbus_wait_pop_frame(bus, frame, &len)) {
        tf_modbus_process_frame(bus, frame, len);
    }
}

void tf_modbus_query_task(void)
{
    tf_modbus_query_step_t step;
    const tf_modbus_query_plan_t *plan;
    bool queued = false;

    if (s_query_step >= (uint32_t)TF_MODBUS_QUERY_STEP_COUNT) {
        s_query_step = 0U;
    }

    step = (tf_modbus_query_step_t)s_query_step;
    s_query_step++;
    if (s_query_step >= (uint32_t)TF_MODBUS_QUERY_STEP_COUNT) {
        s_query_step = 0U;
    }

    if ((uint32_t)step < (sizeof(periodic_query_plan) /
                          sizeof(periodic_query_plan[0]))) {
        plan = &periodic_query_plan[step];
        if ((plan->read != NULL) && (plan->slave_addr != NULL)) {
            queued = plan->read(*plan->slave_addr, plan->start_addr, plan->quantity);
            // if (step >= TF_MODBUS_QUERY_AC_MEASURE) {
            //     TF_DEBUG("APP",
            //              "MODBUS query target=AC step=%u slave=%u start=0x%04X count=%u queued=%u\n",
            //              (unsigned)step,
            //              (unsigned)*plan->slave_addr,
            //              (unsigned)plan->start_addr,
            //              (unsigned)plan->quantity,
            //              queued ? 1U : 0U);
            // }
        }
    }

    if (!queued) {
        /* 入队失败：无任何总线在等响应，仅记录 */
    }
}

static bool tf_modbus_send_next(tf_modbus_bus_t bus)
{
    tf_modbus_master_rt_t *master = &s_bus[bus].master;
    com_link_t *link = &s_bus[bus].link;
    uint8_t *tx_buf = com_link_tx_buffer(link);
    tf_modbus_rsp_expect_t expect;
    uint16_t frame_len = 0U;

    if (com_link_tx_busy(link) || (tx_buf == NULL)) {
        return false;
    }

    if (!tf_modbus_txq_pop(bus, tx_buf, MODULE_MODBUS_TX_BUF_SIZE,
                           &frame_len, &expect)) {
        return false;
    }

    tf_modbus_log_frame("TX", bus, tx_buf, frame_len);
    master->rsp_expect = expect;
    com_link_rx_clear(link);

    if (!com_link_tx_start(link, frame_len)) {
        TF_MODBUS_DIAG_INC(bus, frame_error_count);
        return false;
    }

    tf_modbus_wait_start(bus);
    return true;
}

static uint32_t tf_modbus_response_timeout_ticks(void)
{
    uint32_t timeout_ms = (uint32_t)MODULE_MODBUS_RESPONSE_TIMEOUT_MS;
    uint32_t period_ms = (uint32_t)MODULE_MODBUS_MASTER_PERIOD_MS;
    uint32_t ticks;

    if (period_ms == 0U) {
        return 1U;
    }

    ticks = timeout_ms / period_ms;
    if ((ticks == 0U) || ((timeout_ms % period_ms) != 0U)) {
        ticks++;
    }

    return ticks;
}

void tf_modbus_master_task(void)
{
    uint32_t timeout_ticks = tf_modbus_response_timeout_ticks();

    for (uint32_t bus = 0U; bus < (uint32_t)TF_MODBUS_BUS_COUNT; bus++) {
        tf_modbus_master_rt_t *master = &s_bus[bus].master;
        com_link_t *link = &s_bus[bus].link;

        master->task_tick++;
        com_link_tx_service(link);
        tf_modbus_parse_rx_frames((tf_modbus_bus_t)bus);

        if ((master->state == TF_MODBUS_MASTER_WAIT_RSP) &&
            ((master->task_tick - master->wait_start_tick) >= timeout_ticks)) {
            tf_modbus_device_offline(&master->rsp_expect);
            tf_modbus_wait_done((tf_modbus_bus_t)bus);
            TF_MODBUS_DIAG_INC(bus, timeout_count);
        }

        if ((master->state == TF_MODBUS_MASTER_IDLE) &&
            !com_link_tx_busy(link)) {
            (void)tf_modbus_send_next((tf_modbus_bus_t)bus);
        }
    }
}

void tf_modbus_weather_irq(void)
{
    com_link_irq(&s_bus[TF_MODBUS_BUS_WEATHER].link);
}

void tf_modbus_ac_irq(void)
{
    com_link_irq(&s_bus[TF_MODBUS_BUS_AC].link);
}
