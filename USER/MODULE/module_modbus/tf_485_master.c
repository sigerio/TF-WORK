#include "tf_485_master.h"

#include <string.h>

#include "tf_485.h"
#include "tf_485_txq.h"
#include "tf_485_ac.h"
#include "tf_485_xm7828.h"
#include "tf_485_xm8189.h"
#include "tf_485_xm8537.h"
#include "tf_485_xm9505.h"
#include "../TFSDK/config/tf_board_mcu.h"
#include "tf_delay.h"
#include "tf_debug.h"

/*
 * ─────────────────────────────────────────────────────────────────────
 * tf_485_master — Modbus RTU 主站模块
 * ─────────────────────────────────────────────────────────────────────
 *
 *  层次结构
 * ─────────────────────────────────────────────────────────────────────
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  tf_485_bus_t  (s_bus，全局静态实例)                │
 *  │                                                                 │
 *  │  1. 485 驱动层 (port)                                            │
 *  │     tf_485_t  ← 硬件抽象，DMA 收发 + 环形 FIFO                  │
 *  │     ├─ hw: UART2 + DMA1 句柄                                    │
 *  │     ├─ rx: DMA 接收缓冲区 → IDLE 中断 → 环形 FIFO              │
 *  │     └─ tx: DMA 发送缓冲区 + busy 标志                           │
 *  │                                                                 │
 *  │  2. 主站层 (master_ctx)                                              │
 *  │     tf_485_master_ctx_t  ← 状态机 + 响应匹配         │
 *  │     ├─ 状态: IDLE → WAIT_RESPONSE → IDLE                        │
 *  │     ├─ rsp_expect: 当前等待的响应特征描述                      │
 *  │     └─ 超时计时 + 周期查询步骤遍历                              │
 *  │                                                                 │
 *  │  3. 设备层 (cache)                                             │
 *  │     tf_485_data_cache_t  ← 各从机缓存数据             │
 *  │     ├─ xm7828 / xm8189 / xm8537 / xm9505 / ac                  │
 *  │     └─ 每个设备包含: slave_addr + status(在线/故障) + 数据       │
 *  │                                                                 │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  数据流
 * ─────────────────────────────────────────────────────────────────────
 *
 *  发送路径:
 *    tf_485_query_task() → 按周期查询步骤调用设备 read 接口构建请求帧
 *      → tf_485_txq_push() 入队
 *      → master_task() 空闲时 tf_485_txq_pop() → 拷贝到 tx.dma_buf
 *      → tf_485_tx_start() → DMA → USART → RS485 总线
 *
 *  接收路径:
 *    USART IDLE 中断 → tf_485_irq()
 *      → DMA 停止 → 计算本次接收长度 → ringbuf_write()
 *      → 重启 DMA → master_task() 处理时
 *      → tf_485_wait_pop_frame() 从 FIFO 中按 rsp_expect 匹配帧
 *      → 匹配成功 → CRC 校验 → 设备解析函数 → 更新 cache 缓存
 *
 *  物理层:
 *    USART2 + DMA1 (CH2=RX, CH4=TX)
 *    自动换向 TTL-RS485 转换器 (无需 GPIO 控制 DE/RE)
 * ─────────────────────────────────────────────────────────────────────
 */

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#ifndef MODULE_MODBUS_RESPONSE_TIMEOUT_MS
#define MODULE_MODBUS_RESPONSE_TIMEOUT_MS      60U
#endif

#ifndef MODULE_MODBUS_XM7828_ADDR
#define MODULE_MODBUS_XM7828_ADDR              1U
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
#define MODULE_MODBUS_AC_ADDR                   5U
#endif

typedef enum {
    TF_485_QUERY_XM7828 = 0,
    TF_485_QUERY_XM8189,
    TF_485_QUERY_XM8537,
    TF_485_QUERY_XM9505,
    TF_485_QUERY_AC_MEASURE,
    TF_485_QUERY_AC_HUMIDITY,
    TF_485_QUERY_AC_STATUS,
    TF_485_QUERY_STEP_COUNT,
} tf_485_query_step_t;

/*
 * 主站状态机枚举
 * ─────────────────────────────────────────────────────────────────────
 *  主任务 tf_485_master_task() 按周期运行。
 *
 *  状态流转：
 *    IDLE →（出队成功发帧）→ WAIT_RESPONSE
 *    WAIT_RESPONSE →（收到响应）→ IDLE
 *    WAIT_RESPONSE →（超时）→ IDLE
 * ─────────────────────────────────────────────────────────────────────
 */
typedef enum {
    TF_485_MASTER_IDLE = 0,         /**< 空闲，可发送下一帧                  */
    TF_485_MASTER_WAIT_RSP,    /**< 等待从机响应中                      */
} tf_485_master_state_t;

/**
 * @brief 主站上下文
 *        跟踪主站当前状态、等待的响应信息、以及诊断数据。
 *        生命周期同 s_bus 全局实例。
 */
typedef struct {
#if MODULE_MODBUS_ENABLE_DIAG
    tf_485_diag_t diag;                           /**< 诊断统计信息           */
#endif

    tf_485_master_state_t state;                  /**< 主站状态机当前状态     */
    tf_485_rsp_expect_t rsp_expect;               /**< 当前等待的响应特征描述 */
    uint32_t wait_start_tick;                               /**< 进入等待时的 tick 值   */
    uint32_t task_tick;                                 /**< master_task 累计 tick  */
    uint32_t query_step;                                 /**< 周期查询步骤索引       */
} tf_485_master_ctx_t;

/**
 * @brief 设备数据上下文
 *        缓存所有 Modbus 从机设备的上次成功解析数据。
 *        子模块（如 UI 或协议转换层）通过 tf_485_xxx_get()
 *        接口读取缓存数据，无需直接访问本结构。
 */
typedef struct {
    xm7828_th_data_t xm7828;       /**< XM7828 温湿度数据                      */
    xm8189_wind_data_t xm8189;     /**< XM8189 风速风向数据                    */
    xm8537_water_data_t xm8537;    /**< XM8537 水浸数据                        */
    xm9505_rain_data_t xm9505;     /**< XM9505 雨量数据                        */
    ac_device_t ac;                /**< 空调设备数据                          */
} tf_485_data_cache_t;

/**
 * @brief Modbus 模块顶层上下文
 *        静态全局实例 s_bus，组合了 485 驱动层、主站层、设备层三个层次。
 *
 *  完整嵌套关系：
 *    s_bus: tf_485_bus_t
 *     ├── port:   tf_485_t        ← RS485 驱动上下文，见 tf_485.h
 *     │    ├── hw: tf_485_hw_t    ← UART + DMA 硬件句柄
 *     │    ├── rx: tf_485_rx_t    ← DMA 接收缓冲 + 环形 FIFO
 *     │    └── tx: tf_485_tx_t    ← DMA 发送缓冲 + 忙标志
 *     │
 *     ├── master: tf_485_master_ctx_t  ← 主站状态机、等待响应、诊断
 *     │    ├── state                            ← IDLE / WAIT_RESPONSE
 *     │    ├── rsp_expect                     ← 当前等待的响应期望
 *     │    ├── wait_start_tick / task_tick        ← 超时计时
 *     │    └── query_step                       ← 周期查询步骤进度
 *     │
 *     └── cache: tf_485_data_cache_t  ← 各从机设备缓存数据
 *          ├── xm7828:   xm7828_th_data_t      ← slave_addr + status + temperature + humidity
 *          ├── xm8189:   xm8189_wind_data_t    ← slave_addr + status + speed + direction
 *          ├── xm8537:   xm8537_water_data_t   ← slave_addr + status + raw + water_detected
 *          ├── xm9505:   xm9505_rain_data_t    ← slave_addr + status + rain_count + instant + ...
 *          └── ac:        ac_device_t          ← slave_addr + status + params + run_status
 */
typedef struct {
    tf_485_t port;                             /**< RS485 驱动层（收发硬件 + 缓冲） */
    tf_485_master_ctx_t master;       /**< 主站状态机（状态、响应等待、诊断）*/
    tf_485_data_cache_t cache;      /**< 各从机设备缓存数据               */
} tf_485_bus_t;

typedef bool (*tf_485_query_fn_t)(uint8_t slave_addr,
                                             uint16_t start_reg,
                                             uint16_t reg_count);

typedef struct {
    tf_485_query_fn_t read;
    const uint8_t *slave_addr;
    uint16_t start_addr;
    uint16_t quantity;
} tf_485_query_plan_t;

static tf_485_bus_t s_bus = {
    .cache = {
        .xm7828 = {
            .slave_addr = MODULE_MODBUS_XM7828_ADDR,
        },
        .xm8189 = {
            .slave_addr = MODULE_MODBUS_XM8189_ADDR,
        },
        .xm8537 = {
            .slave_addr = MODULE_MODBUS_XM8537_ADDR,
        },
        .xm9505 = {
            .slave_addr = MODULE_MODBUS_XM9505_ADDR,
        },
        .ac = {
            .slave_addr = MODULE_MODBUS_AC_ADDR,
        },
    },
};

static const tf_485_query_plan_t s_query_plan[TF_485_QUERY_STEP_COUNT] = {
    [TF_485_QUERY_XM7828] = {
        tf_485_xm7828_read,
        &s_bus.cache.xm7828.slave_addr,
        XUNCHIP_REG_DATA0,
        XM7828_MEASUREMENT_REG_COUNT,
    },
    [TF_485_QUERY_XM8189] = {
        tf_485_xm8189_read,
        &s_bus.cache.xm8189.slave_addr,
        XUNCHIP_REG_DATA0,
        XM8189_MEASUREMENT_REG_COUNT,
    },
    [TF_485_QUERY_XM8537] = {
        tf_485_xm8537_read,
        &s_bus.cache.xm8537.slave_addr,
        XUNCHIP_REG_DATA0,
        XM8537_MEASUREMENT_REG_COUNT,
    },
    [TF_485_QUERY_XM9505] = {
        tf_485_xm9505_read,
        &s_bus.cache.xm9505.slave_addr,
        XUNCHIP_REG_DATA0,
        XM9505_MEASUREMENT_REG_COUNT,
    },
    [TF_485_QUERY_AC_MEASURE] = {
        tf_485_ac_read_params,
        &s_bus.cache.ac.slave_addr,
        AC_PARAM_SEG_MEASURE_START,
        AC_PARAM_SEG_MEASURE_COUNT,
    },
    [TF_485_QUERY_AC_HUMIDITY] = {
        tf_485_ac_read_params,
        &s_bus.cache.ac.slave_addr,
        AC_PARAM_SEG_HUMIDITY_START,
        AC_PARAM_SEG_HUMIDITY_COUNT,
    },
    [TF_485_QUERY_AC_STATUS] = {
        tf_485_ac_read_status,
        &s_bus.cache.ac.slave_addr,
        AC_STATUS_SEG_ALL_START,
        AC_STATUS_SEG_ALL_COUNT,
    },
};

#if MODULE_MODBUS_ENABLE_DIAG
#define TF_485_DIAG_INC(field_) (s_bus.master.diag.field_++)
#define TF_485_MASTER_DIAG_INC(master_, field_) ((master_)->diag.field_++)
#else
#define TF_485_DIAG_INC(field_) ((void)0)
#define TF_485_MASTER_DIAG_INC(master_, field_) ((void)(master_))
#endif

static void tf_485_reset_devices(void)
{
    tf_485_data_cache_t *cache = &s_bus.cache;

    memset(&cache->xm7828.status, 0, sizeof(cache->xm7828.status));
    memset(&cache->xm8189.status, 0, sizeof(cache->xm8189.status));
    memset(&cache->xm8537.status, 0, sizeof(cache->xm8537.status));
    memset(&cache->xm9505.status, 0, sizeof(cache->xm9505.status));
    memset(&cache->ac.status, 0, sizeof(cache->ac.status));
}

static void tf_485_reset_runtime(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;

    tf_485_reset(&s_bus.port);
    tf_485_txq_reset();

#if MODULE_MODBUS_ENABLE_DIAG
    memset(&master_ctx->diag, 0, sizeof(master_ctx->diag));
#endif
    memset(&master_ctx->rsp_expect, 0, sizeof(master_ctx->rsp_expect));
    master_ctx->state = TF_485_MASTER_IDLE;
    master_ctx->wait_start_tick = 0U;
    master_ctx->task_tick = 0U;
    master_ctx->query_step = 0U;

    tf_485_reset_devices();
}

static void tf_485_wait_start(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;

    master_ctx->state = TF_485_MASTER_WAIT_RSP;
    master_ctx->wait_start_tick = master_ctx->task_tick;
    TF_485_MASTER_DIAG_INC(master_ctx, request_count);
}

static void tf_485_master_clear_rx(void)
{
    tf_485_rx_clear(&s_bus.port);
}

static uint32_t tf_485_master_peek_rx(uint8_t *buf, uint32_t len)
{
    return tf_485_rx_peek(&s_bus.port, buf, len);
}

static void tf_485_wait_done(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;

    master_ctx->state = TF_485_MASTER_IDLE;
}

static void tf_485_ac_update_fault(void)
{
    tf_485_data_cache_t *cache = &s_bus.cache;
    ac_status_t *status = &cache->ac.run_status;

    cache->ac.status.sensor_fault = (status->cabinet_sensor_fault ||
                                       status->evaporator_sensor_fault ||
                                       status->condenser_sensor_fault ||
                                       status->humidity_sensor_fault);
}

static tf_485_dev_status_t *tf_485_status_of(tf_485_dev_type_t dev_type)
{
    tf_485_data_cache_t *cache = &s_bus.cache;

    switch (dev_type) {
    case TF_485_DEV_XM7828_TH:
        return &cache->xm7828.status;

    case TF_485_DEV_XM8189_WIND:
        return &cache->xm8189.status;

    case TF_485_DEV_XM8537_WATER:
        return &cache->xm8537.status;

    case TF_485_DEV_XM9505_RAIN:
        return &cache->xm9505.status;

    case TF_485_DEV_AC:
        return &cache->ac.status;

    default:
        return NULL;
    }
}

static void tf_485_device_offline(const tf_485_rsp_expect_t *expect)
{
    tf_485_dev_status_t *status;

    if (expect == NULL) {
        return;
    }

    status = tf_485_status_of(expect->dev_type);
    if (status != NULL) {
        status->online = false;
        status->sensor_fault = false;
    }
}

static void tf_485_device_online(const tf_485_rsp_expect_t *expect)
{
    tf_485_dev_status_t *status;

    if (expect == NULL) {
        return;
    }

    status = tf_485_status_of(expect->dev_type);
    if (status != NULL) {
        status->online = true;
    }
}

static bool tf_485_req_is_read(tf_485_req_type_t request_type)
{
    return (request_type == TF_485_REQ_READ_REGS) ||
           (request_type == TF_485_REQ_READ_BITS);
}

static bool tf_485_req_is_write(tf_485_req_type_t request_type)
{
    return (request_type == TF_485_REQ_WRITE_COIL) ||
           (request_type == TF_485_REQ_WRITE_REG);
}



bool tf_485_master_init(void)
{
    if (!tf_485_init(&s_bus.port)) {
        return false;
    }

    tf_485_reset_runtime();
    tf_time_init();

    return true;
}

#if MODULE_MODBUS_ENABLE_DIAG
void tf_485_get_diag(tf_485_diag_t *diag)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    tf_485_txq_diag_t queue_diag;

    if (diag == NULL) {
        return;
    }

    *diag = master_ctx->diag;
    tf_485_txq_get_diag(&queue_diag);
    diag->task_tick = master_ctx->task_tick;
    diag->txq_write_pos = queue_diag.write_index;
    diag->enqueue_count = queue_diag.enqueue_count;
    diag->dequeue_count = queue_diag.dequeue_count;
    diag->txq_overwrite_drop_count = queue_diag.overwrite_drop_count;
    diag->txq_full_drop_count = queue_diag.full_drop_count;
    diag->txq_invalid_count = queue_diag.invalid_count;
}

void tf_485_clear_diag(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;

    memset(&master_ctx->diag, 0, sizeof(master_ctx->diag));
    tf_485_txq_clear_diag();
}
#endif

static bool tf_485_copy_cache(void *out, const void *cache, uint32_t size)
{
    if ((out == NULL) || (cache == NULL) || (size == 0U)) {
        return false;
    }

    memcpy(out, cache, size);
    return true;
}

bool tf_485_xm7828_get(xm7828_th_data_t *out)
{
    return tf_485_copy_cache(out, &s_bus.cache.xm7828, sizeof(*out));
}

bool tf_485_xm8189_get(xm8189_wind_data_t *out)
{
    return tf_485_copy_cache(out, &s_bus.cache.xm8189, sizeof(*out));
}

bool tf_485_xm8537_get(xm8537_water_data_t *out)
{
    return tf_485_copy_cache(out, &s_bus.cache.xm8537, sizeof(*out));
}

bool tf_485_xm9505_get(xm9505_rain_data_t *out)
{
    return tf_485_copy_cache(out, &s_bus.cache.xm9505, sizeof(*out));
}

bool tf_485_ac_get_params(ac_params_t *out)
{
    ac_params_t *cache = &s_bus.cache.ac.params;

    if (!tf_485_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return (cache->marks.raw != 0U);
}

bool tf_485_ac_get_status(ac_status_t *out)
{
    ac_status_t *cache = &s_bus.cache.ac.run_status;

    if (!tf_485_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return (cache->marks.raw != 0U);
}

bool tf_485_ac_get(ac_device_t *out)
{
    ac_device_t *cache = &s_bus.cache.ac;

    if (!tf_485_copy_cache(out, cache, sizeof(*out))) {
        return false;
    }

    return cache->status.online;
}

static uint32_t tf_485_master_rx_available(void)
{
    return tf_485_rx_available(&s_bus.port);
}

static uint32_t tf_485_master_rx_drop(uint32_t len)
{
    return tf_485_rx_drop(&s_bus.port, len);
}

static bool tf_485_wait_pop_frame(uint8_t *frame, uint32_t *frame_len)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    tf_485_rsp_expect_t *expect = &master_ctx->rsp_expect;
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
        (master_ctx->state != TF_485_MASTER_WAIT_RSP)) {
        return false;
    }

    read_request = tf_485_req_is_read(expect->req_type);
    write_request = tf_485_req_is_write(expect->req_type);

    if ((expect->slave_addr == 0U) || (!read_request && !write_request)) {
        tf_485_device_offline(expect);
        TF_485_MASTER_DIAG_INC(master_ctx, frame_error_count);
        tf_485_wait_done();
        return false;
    }

    expect_function = expect->rsp_func;
    expect_byte_count = expect->rsp_data_bytes;
    if (read_request) {
        if ((expect_byte_count == 0U) ||
            (((uint32_t)expect_byte_count + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
            tf_485_device_offline(expect);
            TF_485_MASTER_DIAG_INC(master_ctx, frame_error_count);
            tf_485_wait_done();
            return false;
        }
        expected_len = (uint32_t)expect_byte_count + 5U;
    } else {
        expected_len = MODBUS_RTU_WRITE_SINGLE_RESPONSE_LEN;
    }
    scan_budget = tf_485_master_rx_available();

    while (scan_budget >= 2U) {
        available = tf_485_master_rx_available();
        if (available < 2U) {
            return false;
        }

        if (tf_485_master_peek_rx(head, 2U) != 2U) {
            return false;
        }

        exception_response = (head[1] == (uint8_t)(expect_function | 0x80U));
        if ((head[0] != expect->slave_addr) ||
            ((head[1] != expect_function) && !exception_response)) {
            if (tf_485_master_rx_drop(1U) != 1U) {
                return false;
            }
            TF_485_MASTER_DIAG_INC(master_ctx, unmatched_count);
            scan_budget--;
            continue;
        }

        if (exception_response) {
            expected_len = MODBUS_RTU_EXCEPTION_LEN;
            available = tf_485_master_rx_available();
            if (available < expected_len) {
                return false;
            }

            if (tf_485_master_peek_rx(frame, expected_len) != expected_len) {
                return false;
            }

            (void)tf_485_master_rx_drop(expected_len);
            *frame_len = expected_len;
            return true;
        }

        if (read_request) {
            available = tf_485_master_rx_available();
            if (available < 3U) {
                return false;
            }

            if (tf_485_master_peek_rx(head, 3U) != 3U) {
                return false;
            }

            if (head[2] != expect_byte_count) {
                (void)tf_485_master_rx_drop(3U);
                tf_485_device_offline(expect);
                TF_485_MASTER_DIAG_INC(master_ctx, frame_error_count);
                tf_485_wait_done();
                return false;
            }
        }

        available = tf_485_master_rx_available();
        if (available < expected_len) {
            return false;
        }

        if (tf_485_master_peek_rx(frame, expected_len) != expected_len) {
            return false;
        }

        (void)tf_485_master_rx_drop(expected_len);
        *frame_len = expected_len;
        return true;
    }

    return false;
}

typedef modbus_rtu_status_t (*tf_485_rsp_parse_fn_t)(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache);

static modbus_rtu_status_t tf_485_parse_wait_xm_read(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case TF_485_DEV_XM7828_TH:
        return tf_485_xm7828_parse(frame, len, expect->slave_addr,
                                   &cache->xm7828);

    case TF_485_DEV_XM8189_WIND:
        return tf_485_xm8189_parse(frame, len, expect->slave_addr,
                                   &cache->xm8189);

    case TF_485_DEV_XM8537_WATER:
        return tf_485_xm8537_parse(frame, len, expect->slave_addr,
                                   &cache->xm8537);

    case TF_485_DEV_XM9505_RAIN:
        return tf_485_xm9505_parse(frame, len, expect->slave_addr,
                                   &cache->xm9505);

    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t tf_485_parse_wait_xm_write(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case TF_485_DEV_XM7828_TH:
        return tf_485_xm7828_parse_write_ack(frame, len, expect->slave_addr,
                                             expect->rsp_func,
                                             expect->echo_addr,
                                             expect->echo_value,
                                             &cache->xm7828);

    case TF_485_DEV_XM8189_WIND:
        return tf_485_xm8189_parse_write_ack(frame, len, expect->slave_addr,
                                             expect->rsp_func,
                                             expect->echo_addr,
                                             expect->echo_value,
                                             &cache->xm8189);

    case TF_485_DEV_XM8537_WATER:
        return tf_485_xm8537_parse_write_ack(frame, len, expect->slave_addr,
                                             expect->rsp_func,
                                             expect->echo_addr,
                                             expect->echo_value,
                                             &cache->xm8537);

    case TF_485_DEV_XM9505_RAIN:
        return tf_485_xm9505_parse_write_ack(frame, len, expect->slave_addr,
                                             expect->rsp_func,
                                             expect->echo_addr,
                                             expect->echo_value,
                                             &cache->xm9505);

    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t tf_485_parse_wait_ac_regs(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache)
{
    return tf_485_ac_parse_params(frame,
                                         len,
                                         expect->slave_addr,
                                         expect->start_addr,
                                         expect->quantity,
                                         &cache->ac.params);
}

static modbus_rtu_status_t tf_485_parse_wait_ac_bits(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache)
{
    return tf_485_ac_parse_status(frame,
                                         len,
                                         expect->slave_addr,
                                         expect->start_addr,
                                         &cache->ac.run_status);
}

static modbus_rtu_status_t tf_485_parse_wait_ac_write(
    const uint8_t *frame,
    uint32_t len,
    const tf_485_rsp_expect_t *expect,
    tf_485_data_cache_t *cache)
{
    return tf_485_ac_parse_write_ack(frame,
                                            len,
                                            expect->slave_addr,
                                            expect->rsp_func,
                                            expect->echo_addr,
                                            expect->echo_value,
                                            &cache->ac);
}

static const tf_485_rsp_parse_fn_t
    s_rsp_parse_table[TF_485_DEV_COUNT][TF_485_REQ_COUNT] = {
    [TF_485_DEV_XM7828_TH] = {
        [TF_485_REQ_READ_REGS] = tf_485_parse_wait_xm_read,
        [TF_485_REQ_WRITE_REG] = tf_485_parse_wait_xm_write,
    },
    [TF_485_DEV_XM8189_WIND] = {
        [TF_485_REQ_READ_REGS] = tf_485_parse_wait_xm_read,
        [TF_485_REQ_WRITE_REG] = tf_485_parse_wait_xm_write,
    },
    [TF_485_DEV_XM8537_WATER] = {
        [TF_485_REQ_READ_REGS] = tf_485_parse_wait_xm_read,
        [TF_485_REQ_WRITE_REG] = tf_485_parse_wait_xm_write,
    },
    [TF_485_DEV_XM9505_RAIN] = {
        [TF_485_REQ_READ_REGS] = tf_485_parse_wait_xm_read,
        [TF_485_REQ_WRITE_REG] = tf_485_parse_wait_xm_write,
    },
    [TF_485_DEV_AC] = {
        [TF_485_REQ_READ_REGS] = tf_485_parse_wait_ac_regs,
        [TF_485_REQ_READ_BITS] = tf_485_parse_wait_ac_bits,
        [TF_485_REQ_WRITE_COIL] = tf_485_parse_wait_ac_write,
        [TF_485_REQ_WRITE_REG] = tf_485_parse_wait_ac_write,
    },
};

static modbus_rtu_status_t tf_485_parse_waiting(const uint8_t *frame,
                                                                uint32_t len)
{
    tf_485_rsp_expect_t *expect = &s_bus.master.rsp_expect;
    tf_485_data_cache_t *cache = &s_bus.cache;
    tf_485_rsp_parse_fn_t parse;

    if (((uint32_t)expect->dev_type >= (uint32_t)TF_485_DEV_COUNT) ||
        ((uint32_t)expect->req_type >= (uint32_t)TF_485_REQ_COUNT)) {
        return MODBUS_RTU_ERR_FUNC;
    }

    parse = s_rsp_parse_table[expect->dev_type][expect->req_type];
    if (parse == NULL) {
        return MODBUS_RTU_ERR_FUNC;
    }

    return parse(frame, len, expect, cache);
}

static void tf_485_count_parse_error(modbus_rtu_status_t status)
{
    if (status == MODBUS_RTU_ERR_CRC) {
        TF_485_DIAG_INC(crc_error_count);
    } else {
        TF_485_DIAG_INC(parse_error_count);
    }
}

static void tf_485_process_frame(const uint8_t *frame, uint32_t len)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    tf_485_rsp_expect_t *expect = &master_ctx->rsp_expect;
    uint8_t expect_function;
    modbus_rtu_status_t status;

    expect_function = expect->rsp_func;
    TF_485_MASTER_DIAG_INC(master_ctx, response_count);
    if (frame[1] == (uint8_t)(expect_function | 0x80U)) {
        modbus_rtu_exception_t exception;

        status = modbus_rtu_parse_exception(frame,
                                            len,
                                            expect->slave_addr,
                                            expect_function,
                                            &exception);
        if (status == MODBUS_RTU_OK) {
            TF_DEBUG("APP",
                     "modbus exception slave_addr=%u func=0x%02X code=0x%02X dev=%u start=0x%04X count=%u\n",
                     exception.address,
                     exception.function,
                     exception.exception_code,
                     (uint8_t)expect->dev_type,
                     expect->start_addr,
                     expect->quantity);
        } else {
            TF_DEBUG("APP",
                     "modbus exception parse failed status=%u slave_addr=%u func=0x%02X len=%lu\n",
                     (uint8_t)status,
                     expect->slave_addr,
                     expect_function,
                     (unsigned long)len);
        }
        tf_485_count_parse_error(status);
        if ((status == MODBUS_RTU_OK) &&
            tf_485_req_is_write(expect->req_type)) {
            tf_485_device_online(expect);
        } else {
            tf_485_device_offline(expect);
        }
        tf_485_wait_done();
        return;
    }

    status = tf_485_parse_waiting(frame, len);
    if (status == MODBUS_RTU_OK) {
        if (expect->dev_type == TF_485_DEV_AC) {
            tf_485_device_online(expect);
            if (expect->req_type == TF_485_REQ_READ_BITS) {
                tf_485_ac_update_fault();
            }
        }
        TF_485_MASTER_DIAG_INC(master_ctx, parse_ok_count);
    } else {
        tf_485_device_offline(expect);
        tf_485_count_parse_error(status);
    }

    tf_485_wait_done();
}

static void tf_485_parse_rx_frames(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    uint8_t frame[MODULE_MODBUS_RX_BUF_SIZE];
    uint32_t len = 0U;

    if (master_ctx->state != TF_485_MASTER_WAIT_RSP) {
        return;
    }

    if (tf_485_wait_pop_frame(frame, &len)) {
        tf_485_process_frame(frame, len);
    }
}

void tf_485_query_task(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    tf_485_query_step_t step;
    const tf_485_query_plan_t *plan;
    bool queued = false;

    if (master_ctx->query_step >= TF_485_QUERY_STEP_COUNT) {
        master_ctx->query_step = 0U;
    }

    step = (tf_485_query_step_t)master_ctx->query_step;
    master_ctx->query_step++;
    if (master_ctx->query_step >= TF_485_QUERY_STEP_COUNT) {
        master_ctx->query_step = 0U;
    }

    if ((uint32_t)step < (sizeof(s_query_plan) /
                          sizeof(s_query_plan[0]))) {
        plan = &s_query_plan[step];
        if ((plan->read != NULL) && (plan->slave_addr != NULL)) {
            queued = plan->read(*plan->slave_addr, plan->start_addr, plan->quantity);
        }
    }

    if (!queued) {
        TF_485_MASTER_DIAG_INC(master_ctx, frame_error_count);
    }
}

static bool tf_485_send_next(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    uint8_t *tx_buf = tf_485_tx_buffer(&s_bus.port);
    tf_485_rsp_expect_t expect;
    uint16_t frame_len = 0U;

    if (tf_485_tx_busy(&s_bus.port)) {
        return false;
    }

    if (tx_buf == NULL) {
        return false;
    }

    if (!tf_485_txq_pop(tx_buf,
                                 MODULE_MODBUS_TX_BUF_SIZE,
                                 &frame_len,
                                 &expect)) {
        return false;
    }

    s_bus.master.rsp_expect = expect;
    tf_485_master_clear_rx();

    if (!tf_485_tx_start(&s_bus.port, frame_len)) {
        TF_485_MASTER_DIAG_INC(master_ctx, frame_error_count);
        return false;
    }

    tf_485_wait_start();
    return true;
}

static uint32_t tf_485_response_timeout_ticks(void)
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

void tf_485_master_task(void)
{
    tf_485_master_ctx_t *master_ctx = &s_bus.master;
    uint32_t timeout_ticks = tf_485_response_timeout_ticks();

    master_ctx->task_tick++;
    tf_485_tx_service(&s_bus.port);
    tf_485_parse_rx_frames();

    if ((master_ctx->state == TF_485_MASTER_WAIT_RSP) &&
        ((master_ctx->task_tick - master_ctx->wait_start_tick) >= timeout_ticks)) {
        tf_485_device_offline(&master_ctx->rsp_expect);
        tf_485_wait_done();
        TF_485_MASTER_DIAG_INC(master_ctx, timeout_count);
    }

    if ((master_ctx->state == TF_485_MASTER_IDLE) &&
        !tf_485_tx_busy(&s_bus.port)) {
        (void)tf_485_send_next();
    }
}

void tf_485_usart2_irq(void)
{
    tf_485_irq(&s_bus.port);
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
