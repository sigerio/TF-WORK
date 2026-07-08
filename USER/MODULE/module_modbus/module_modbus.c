#include "module_modbus.h"

#include <string.h>

#include "modbus_rs485_bus.h"
#include "modbus_tx_queue.h"
#include "module_modbus_ac.h"
#include "module_modbus_xm7828.h"
#include "module_modbus_xm8189.h"
#include "module_modbus_xm8537.h"
#include "module_modbus_xm9505.h"
#include "../TFSDK/config/tf_board_mcu.h"
#include "tf_delay.h"
#include "tf_debug.h"

/*
 * ─────────────────────────────────────────────────────────────────────
 * module_modbus — Modbus RTU 主站模块
 * ─────────────────────────────────────────────────────────────────────
 *
 *  层次结构
 * ─────────────────────────────────────────────────────────────────────
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  module_modbus_context_t  (s_modbus，全局静态实例)                │
 *  │                                                                 │
 *  │  1. 总线层 (bus)                                                │
 *  │     modbus_rs485_bus_t  ← 硬件抽象，DMA 收发 + 环形 FIFO        │
 *  │     ├─ hw: UART2 + DMA1 句柄                                    │
 *  │     ├─ rx: DMA 接收缓冲区 → IDLE 中断 → 环形 FIFO              │
 *  │     └─ tx: DMA 发送缓冲区 + busy 标志                           │
 *  │                                                                 │
 *  │  2. 主站层 (master)                                              │
 *  │     module_modbus_master_context_t  ← 状态机 + 响应匹配         │
 *  │     ├─ 状态: IDLE → WAIT_RESPONSE → IDLE                        │
 *  │     ├─ waiting_item: 当前等待的响应特征描述                      │
 *  │     └─ 超时计时 + 固定轮询步骤遍历                              │
 *  │                                                                 │
 *  │  3. 设备层 (devices)                                             │
 *  │     module_modbus_device_context_t  ← 各从机缓存数据             │
 *  │     ├─ xm7828 / xm8189 / xm8537 / xm9505 / ac                  │
 *  │     └─ 每个设备包含: addr + status(在线/故障) + 数据            │
 *  │                                                                 │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  数据流
 * ─────────────────────────────────────────────────────────────────────
 *
 *  发送路径:
 *    queue_fill_task() → 按固定轮询步骤调用设备 read 接口构建请求帧
 *      → modbus_tx_queue_enqueue() 入队
 *      → master_task() 空闲时 dequeue() → 拷贝到 tx.dma_buf
 *      → modbus_rs485_start_tx_dma() → DMA → USART → RS485 总线
 *
 *  接收路径:
 *    USART IDLE 中断 → modbus_rs485_usart_irq_handler()
 *      → DMA 停止 → 计算本次接收长度 → ringbuf_write()
 *      → 重启 DMA → master_task() 轮询时
 *      → pop_waiting_response() 从 ring 中按 waiting_item 匹配帧
 *      → 匹配成功 → CRC 校验 → 设备解析函数 → 更新 devices 缓存
 *
 *  物理层:
 *    USART2 + DMA1 (CH2=RX, CH4=TX)
 *    自动换向 TTL-RS485 转换器 (无需 GPIO 控制 DE/RE)
 * ─────────────────────────────────────────────────────────────────────
 */

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#ifndef MODULE_MODBUS_POLL_PERIOD_MS
#define MODULE_MODBUS_POLL_PERIOD_MS           MODULE_MODBUS_MASTER_PERIOD_MS
#endif

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
    MODULE_MODBUS_POLL_STEP_XM7828 = 0,
    MODULE_MODBUS_POLL_STEP_XM8189,
    MODULE_MODBUS_POLL_STEP_XM8537,
    MODULE_MODBUS_POLL_STEP_XM9505,
    MODULE_MODBUS_POLL_STEP_AC_MEASURE_PARAMS,
    MODULE_MODBUS_POLL_STEP_AC_HUMIDITY_PARAM,
    MODULE_MODBUS_POLL_STEP_AC_STATUS,
    MODULE_MODBUS_POLL_STEP_COUNT,
} module_modbus_poll_step_t;

/*
 * 主站状态机枚举
 * ─────────────────────────────────────────────────────────────────────
 *  主任务 module_modbus_master_task() 按周期运行。
 *
 *  状态流转：
 *    IDLE →（出队成功发帧）→ WAIT_RESPONSE
 *    WAIT_RESPONSE →（收到响应）→ IDLE
 *    WAIT_RESPONSE →（超时）→ IDLE
 * ─────────────────────────────────────────────────────────────────────
 */
typedef enum {
    MODULE_MODBUS_MASTER_IDLE = 0,         /**< 空闲，可发送下一帧                  */
    MODULE_MODBUS_MASTER_WAIT_RESPONSE,    /**< 等待从机响应中                      */
} module_modbus_master_state_t;

/**
 * @brief 主站上下文
 *        跟踪主站当前状态、等待的响应信息、以及诊断数据。
 *        生命周期同 s_modbus 全局实例。
 */
typedef struct {
#if MODULE_MODBUS_ENABLE_DIAG
    module_modbus_diag_t diag;                           /**< 诊断统计信息           */
#endif

    module_modbus_master_state_t state;                  /**< 主站状态机当前状态     */
    modbus_response_expect_t waiting_item;               /**< 当前等待的响应特征描述 */
    uint32_t waiting_tick;                               /**< 进入等待时的 tick 值   */
    uint32_t tick_count;                                 /**< master_task 累计 tick  */
    uint32_t poll_index;                                 /**< 固定轮询步骤索引       */
} module_modbus_master_context_t;

/**
 * @brief 设备数据上下文
 *        缓存所有 Modbus 从机设备的上次成功解析数据。
 *        子模块（如 UI 或协议转换层）通过 module_modbus_get_xxx_data()
 *        接口读取缓存数据，无需直接访问本结构。
 */
typedef struct {
    xm7828_th_data_t xm7828;       /**< XM7828 温湿度数据                      */
    xm8189_wind_data_t xm8189;     /**< XM8189 风速风向数据                    */
    xm8537_water_data_t xm8537;    /**< XM8537 水浸数据                        */
    xm9505_rain_data_t xm9505;     /**< XM9505 雨量数据                        */
    ac_device_t ac;                /**< 空调设备数据                          */
} module_modbus_device_context_t;

/**
 * @brief Modbus 模块顶层上下文
 *        静态全局实例 s_modbus，组合了总线层、主站层、设备层三个层次。
 *
 *  完整嵌套关系：
 *    s_modbus: module_modbus_context_t
 *     ├── bus:  modbus_rs485_bus_t          ← RS485 总线，见 modbus_rs485_bus.h
 *     │    ├── hw: modbus_rs485_hw_t        ← UART + DMA 硬件句柄
 *     │    ├── rx: modbus_rs485_rx_t        ← DMA 接收缓冲 + 环形 FIFO
 *     │    └── tx: modbus_rs485_tx_t        ← DMA 发送缓冲 + 忙标志
 *     │
 *     ├── master: module_modbus_master_context_t  ← 主站状态机、等待响应、诊断
 *     │    ├── state                            ← IDLE / WAIT_RESPONSE
 *     │    ├── waiting_item                     ← 当前等待的响应期望
 *     │    ├── waiting_tick / tick_count        ← 超时计时
 *     │    └── poll_index                       ← 固定轮询步骤进度
 *     │
 *     └── devices: module_modbus_device_context_t  ← 各从机设备缓存数据
 *          ├── xm7828:   xm7828_th_data_t      ← addr + status + temperature + humidity
 *          ├── xm8189:   xm8189_wind_data_t    ← addr + status + speed + direction
 *          ├── xm8537:   xm8537_water_data_t   ← addr + status + raw + water_detected
 *          ├── xm9505:   xm9505_rain_data_t    ← addr + status + rain_count + instant + ...
 *          └── ac:        ac_device_t          ← addr + status + params + run_status
 */
typedef struct {
    modbus_rs485_bus_t bus;                      /**< RS485 总线层（收发硬件 + 缓冲） */
    module_modbus_master_context_t master;       /**< 主站状态机（状态、响应等待、诊断）*/
    module_modbus_device_context_t devices;      /**< 各从机设备缓存数据               */
} module_modbus_context_t;

typedef bool (*module_modbus_poll_read_fn_t)(uint8_t addr,
                                             uint16_t start_reg,
                                             uint16_t reg_count);

typedef struct {
    module_modbus_poll_read_fn_t read;
    const uint8_t *addr;
    uint16_t start;
    uint16_t count;
} module_modbus_poll_step_map_t;

static module_modbus_context_t s_modbus = {
    .devices = {
        .xm7828 = {
            .addr = MODULE_MODBUS_XM7828_ADDR,
        },
        .xm8189 = {
            .addr = MODULE_MODBUS_XM8189_ADDR,
        },
        .xm8537 = {
            .addr = MODULE_MODBUS_XM8537_ADDR,
        },
        .xm9505 = {
            .addr = MODULE_MODBUS_XM9505_ADDR,
        },
        .ac = {
            .addr = MODULE_MODBUS_AC_ADDR,
        },
    },
};

static const module_modbus_poll_step_map_t s_modbus_poll_step_map[MODULE_MODBUS_POLL_STEP_COUNT] = {
    [MODULE_MODBUS_POLL_STEP_XM7828] = {
        module_modbus_xm7828_read_measurements,
        &s_modbus.devices.xm7828.addr,
        XUNCHIP_REG_DATA0,
        XM7828_MEASUREMENT_REG_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_XM8189] = {
        module_modbus_xm8189_read,
        &s_modbus.devices.xm8189.addr,
        XUNCHIP_REG_DATA0,
        XM8189_MEASUREMENT_REG_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_XM8537] = {
        module_modbus_xm8537_read,
        &s_modbus.devices.xm8537.addr,
        XUNCHIP_REG_DATA0,
        XM8537_MEASUREMENT_REG_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_XM9505] = {
        module_modbus_xm9505_read,
        &s_modbus.devices.xm9505.addr,
        XUNCHIP_REG_DATA0,
        XM9505_MEASUREMENT_REG_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_AC_MEASURE_PARAMS] = {
        module_modbus_ac_read_params,
        &s_modbus.devices.ac.addr,
        AC_PARAM_SEG_MEASURE_START,
        AC_PARAM_SEG_MEASURE_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_AC_HUMIDITY_PARAM] = {
        module_modbus_ac_read_params,
        &s_modbus.devices.ac.addr,
        AC_PARAM_SEG_HUMIDITY_START,
        AC_PARAM_SEG_HUMIDITY_COUNT,
    },
    [MODULE_MODBUS_POLL_STEP_AC_STATUS] = {
        module_modbus_ac_read_status,
        &s_modbus.devices.ac.addr,
        AC_STATUS_SEG_ALL_START,
        AC_STATUS_SEG_ALL_COUNT,
    },
};

#if MODULE_MODBUS_ENABLE_DIAG
#define MODULE_MODBUS_DIAG_INC(field_) (s_modbus.master.diag.field_++)
#define MODULE_MODBUS_MASTER_DIAG_INC(master_, field_) ((master_)->diag.field_++)
#else
#define MODULE_MODBUS_DIAG_INC(field_) ((void)0)
#define MODULE_MODBUS_MASTER_DIAG_INC(master_, field_) ((void)(master_))
#endif

static void module_modbus_reset_device_runtime_state(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    memset(&devices->xm7828.status, 0, sizeof(devices->xm7828.status));
    memset(&devices->xm8189.status, 0, sizeof(devices->xm8189.status));
    memset(&devices->xm8537.status, 0, sizeof(devices->xm8537.status));
    memset(&devices->xm9505.status, 0, sizeof(devices->xm9505.status));
    memset(&devices->ac.status, 0, sizeof(devices->ac.status));
}

static void module_modbus_reset_runtime_state(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;

    modbus_rs485_reset_runtime(&s_modbus.bus);
    modbus_tx_queue_reset();

#if MODULE_MODBUS_ENABLE_DIAG
    memset(&master->diag, 0, sizeof(master->diag));
#endif
    memset(&master->waiting_item, 0, sizeof(master->waiting_item));
    master->state = MODULE_MODBUS_MASTER_IDLE;
    master->waiting_tick = 0U;
    master->tick_count = 0U;
    master->poll_index = 0U;

    module_modbus_reset_device_runtime_state();
}

static void module_modbus_start_waiting_response(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;

    master->state = MODULE_MODBUS_MASTER_WAIT_RESPONSE;
    master->waiting_tick = master->tick_count;
    MODULE_MODBUS_MASTER_DIAG_INC(master, request_count);
}

static void module_modbus_clear_rx_buffers(void)
{
    modbus_rs485_rx_clear(&s_modbus.bus);
}

static uint32_t module_modbus_peek_rx(uint8_t *buf, uint32_t len)
{
    return modbus_rs485_rx_peek(&s_modbus.bus, buf, len);
}

static void module_modbus_finish_waiting_response(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;

    master->state = MODULE_MODBUS_MASTER_IDLE;
}

static void module_modbus_mark_ac_offline(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    devices->ac.status.online = false;
    devices->ac.status.sensor_fault = false;
}

static void module_modbus_mark_ac_online(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    devices->ac.status.online = true;
}

static void module_modbus_update_ac_sensor_fault(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;
    ac_status_t *status = &devices->ac.run_status;

    devices->ac.status.sensor_fault = (status->cabinet_sensor_fault ||
                                       status->evaporator_sensor_fault ||
                                       status->condenser_sensor_fault ||
                                       status->humidity_sensor_fault);
}

static void module_modbus_mark_device_offline(const modbus_response_expect_t *item)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    if (item == NULL) {
        return;
    }

    switch (item->device) {
    case MODULE_MODBUS_DEV_XM7828_TH:
        devices->xm7828.status.online = false;
        devices->xm7828.status.sensor_fault = false;
        break;

    case MODULE_MODBUS_DEV_XM8189_WIND:
        devices->xm8189.status.online = false;
        devices->xm8189.status.sensor_fault = false;
        break;

    case MODULE_MODBUS_DEV_XM8537_WATER:
        devices->xm8537.status.online = false;
        devices->xm8537.status.sensor_fault = false;
        break;

    case MODULE_MODBUS_DEV_XM9505_RAIN:
        devices->xm9505.status.online = false;
        devices->xm9505.status.sensor_fault = false;
        break;

    case MODULE_MODBUS_DEV_AC:
        module_modbus_mark_ac_offline();
        break;

    default:
        break;
    }
}



bool module_modbus_init(void)
{
    if (!modbus_rs485_init(&s_modbus.bus)) {
        return false;
    }

    module_modbus_reset_runtime_state();
    tf_time_init();

    return true;
}

#if MODULE_MODBUS_ENABLE_DIAG
void module_modbus_get_diag(module_modbus_diag_t *diag)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_tx_queue_diag_t queue_diag;

    if (diag == NULL) {
        return;
    }

    *diag = master->diag;
    modbus_tx_queue_get_diag(&queue_diag);
    diag->tick_count = master->tick_count;
    diag->send_buf_write_index = queue_diag.write_index;
    diag->enqueue_count = queue_diag.enqueue_count;
    diag->dequeue_count = queue_diag.dequeue_count;
    diag->queue_overwrite_drop_count = queue_diag.overwrite_drop_count;
    diag->queue_full_drop_count = queue_diag.full_drop_count;
    diag->queue_invalid_count = queue_diag.invalid_count;
}

void module_modbus_clear_diag(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;

    memset(&master->diag, 0, sizeof(master->diag));
    modbus_tx_queue_clear_diag();
}
#endif

bool module_modbus_get_xm7828_data(xm7828_th_data_t *out)
{
    xm7828_th_data_t *cache = &s_modbus.devices.xm7828;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return true;
}

bool module_modbus_get_xm8189_data(xm8189_wind_data_t *out)
{
    xm8189_wind_data_t *cache = &s_modbus.devices.xm8189;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return true;
}

bool module_modbus_get_xm8537_data(xm8537_water_data_t *out)
{
    xm8537_water_data_t *cache = &s_modbus.devices.xm8537;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return true;
}

bool module_modbus_get_xm9505_data(xm9505_rain_data_t *out)
{
    xm9505_rain_data_t *cache = &s_modbus.devices.xm9505;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return true;
}

bool module_modbus_get_ac_params(ac_params_t *out)
{
    ac_params_t *cache = &s_modbus.devices.ac.params;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return (cache->marks.raw != 0U);
}

bool module_modbus_get_ac_status(ac_status_t *out)
{
    ac_status_t *cache = &s_modbus.devices.ac.run_status;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return (cache->marks.raw != 0U);
}

bool module_modbus_get_ac_data(ac_device_t *out)
{
    ac_device_t *cache = &s_modbus.devices.ac;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return cache->status.online;
}

static uint32_t module_modbus_rx_available(void)
{
    return modbus_rs485_rx_available(&s_modbus.bus);
}

static uint32_t module_modbus_rx_drop_bytes(uint32_t len)
{
    return modbus_rs485_rx_drop(&s_modbus.bus, len);
}

static bool module_modbus_pop_waiting_response(uint8_t *frame, uint32_t *frame_len)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_response_expect_t *item = &master->waiting_item;
    uint8_t head[3];
    uint8_t expect_function;
    uint8_t expect_byte_count;
    uint32_t available;
    uint32_t expected_len;
    uint32_t scan_budget;
    bool exception_response;

    if ((frame == NULL) || (frame_len == NULL) ||
        (master->state != MODULE_MODBUS_MASTER_WAIT_RESPONSE)) {
        return false;
    }

    if ((item->addr == 0U) || (item->expected_bytes == 0U) ||
        (((uint32_t)item->expected_bytes + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
        module_modbus_mark_device_offline(item);
        MODULE_MODBUS_MASTER_DIAG_INC(master, frame_error_count);
        module_modbus_finish_waiting_response();
        return false;
    }

    expect_function = item->expected_func;
    expect_byte_count = item->expected_bytes;
    expected_len = (uint32_t)expect_byte_count + 5U;
    scan_budget = module_modbus_rx_available();

    while (scan_budget >= 2U) {
        available = module_modbus_rx_available();
        if (available < 2U) {
            return false;
        }

        if (module_modbus_peek_rx(head, 2U) != 2U) {
            return false;
        }

        exception_response = (head[1] == (uint8_t)(expect_function | 0x80U));
        if ((head[0] != item->addr) ||
            ((head[1] != expect_function) && !exception_response)) {
            if (module_modbus_rx_drop_bytes(1U) != 1U) {
                return false;
            }
            MODULE_MODBUS_MASTER_DIAG_INC(master, unmatched_count);
            scan_budget--;
            continue;
        }

        if (exception_response) {
            expected_len = MODBUS_RTU_EXCEPTION_LEN;
            available = module_modbus_rx_available();
            if (available < expected_len) {
                return false;
            }

            if (module_modbus_peek_rx(frame, expected_len) != expected_len) {
                return false;
            }

            (void)module_modbus_rx_drop_bytes(expected_len);
            *frame_len = expected_len;
            return true;
        }

        available = module_modbus_rx_available();
        if (available < 3U) {
            return false;
        }

        if (module_modbus_peek_rx(head, 3U) != 3U) {
            return false;
        }

        if (head[2] != expect_byte_count) {
            (void)module_modbus_rx_drop_bytes(3U);
            module_modbus_mark_device_offline(item);
            MODULE_MODBUS_MASTER_DIAG_INC(master, frame_error_count);
            module_modbus_finish_waiting_response();
            return false;
        }

        available = module_modbus_rx_available();
        if (available < expected_len) {
            return false;
        }

        if (module_modbus_peek_rx(frame, expected_len) != expected_len) {
            return false;
        }

        (void)module_modbus_rx_drop_bytes(expected_len);
        *frame_len = expected_len;
        return true;
    }

    return false;
}

static modbus_rtu_status_t module_modbus_parse_waiting_response(const uint8_t *frame,
                                                                uint32_t len)
{
    modbus_response_expect_t *item = &s_modbus.master.waiting_item;
    module_modbus_device_context_t *devices = &s_modbus.devices;
    uint16_t regs[MODBUS_RTU_MAX_REG_COUNT];
    uint8_t bits[64];
    uint16_t parsed_count = 0U;
    uint8_t expect_function = item->expected_func;

    switch (item->device) {
    case MODULE_MODBUS_DEV_XM7828_TH:
        return module_modbus_parse_xm7828(frame, len, item->addr, &devices->xm7828);

    case MODULE_MODBUS_DEV_XM8189_WIND:
        return module_modbus_parse_xm8189(frame, len, item->addr, &devices->xm8189);

    case MODULE_MODBUS_DEV_XM8537_WATER:
        return module_modbus_parse_xm8537(frame, len, item->addr, &devices->xm8537);

    case MODULE_MODBUS_DEV_XM9505_RAIN:
        return module_modbus_parse_xm9505(frame, len, item->addr, &devices->xm9505);

    case MODULE_MODBUS_DEV_AC:
        if (item->function == MODULE_MODBUS_POLL_READ_BITS) {
            return module_modbus_ac_parse_status(frame,
                                                 len,
                                                 item->addr,
                                                 item->start,
                                                 &devices->ac.run_status);
        }
        return module_modbus_ac_parse_params(frame,
                                             len,
                                             item->addr,
                                             item->start,
                                             item->count,
                                             &devices->ac.params);

    case MODULE_MODBUS_DEV_CUSTOM:
    default:
        if (item->function == MODULE_MODBUS_POLL_READ_BITS) {
            return modbus_rtu_parse_read_bits_response(frame,
                                                       len,
                                                       item->addr,
                                                       expect_function,
                                                       bits,
                                                       (uint16_t)sizeof(bits),
                                                       &parsed_count);
        }
        return modbus_rtu_parse_read_regs_response(frame,
                                                   len,
                                                   item->addr,
                                                   expect_function,
                                                   regs,
                                                   MODBUS_RTU_MAX_REG_COUNT,
                                                   &parsed_count);
    }
}

static void module_modbus_count_parse_error(modbus_rtu_status_t status)
{
    if (status == MODBUS_RTU_ERR_CRC) {
        MODULE_MODBUS_DIAG_INC(crc_error_count);
    } else {
        MODULE_MODBUS_DIAG_INC(parse_error_count);
    }
}

static void module_modbus_process_frame(const uint8_t *frame, uint32_t len)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_response_expect_t *item = &master->waiting_item;
    uint8_t expect_function;
    modbus_rtu_status_t status;

    expect_function = item->expected_func;
    MODULE_MODBUS_MASTER_DIAG_INC(master, response_count);
    if (frame[1] == (uint8_t)(expect_function | 0x80U)) {
        modbus_rtu_exception_t exception;

        status = modbus_rtu_parse_exception(frame,
                                            len,
                                            item->addr,
                                            expect_function,
                                            &exception);
        if (status == MODBUS_RTU_OK) {
            TF_DEBUG("APP",
                     "modbus exception addr=%u func=0x%02X code=0x%02X dev=%u start=0x%04X count=%u\n",
                     exception.address,
                     exception.function,
                     exception.exception_code,
                     (uint8_t)item->device,
                     item->start,
                     item->count);
        } else {
            TF_DEBUG("APP",
                     "modbus exception parse failed status=%u addr=%u func=0x%02X len=%lu\n",
                     (uint8_t)status,
                     item->addr,
                     expect_function,
                     (unsigned long)len);
        }
        module_modbus_count_parse_error(status);
        module_modbus_mark_device_offline(item);
        module_modbus_finish_waiting_response();
        return;
    }

    status = module_modbus_parse_waiting_response(frame, len);
    if (status == MODBUS_RTU_OK) {
        if (item->device == MODULE_MODBUS_DEV_AC) {
            module_modbus_mark_ac_online();
            if (item->function == MODULE_MODBUS_POLL_READ_BITS) {
                module_modbus_update_ac_sensor_fault();
            }
        }
        MODULE_MODBUS_MASTER_DIAG_INC(master, parse_ok_count);
    } else {
        module_modbus_mark_device_offline(item);
        module_modbus_count_parse_error(status);
    }

    module_modbus_finish_waiting_response();
}

static void module_modbus_parse_ringbuf_frames(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    uint8_t frame[MODULE_MODBUS_RX_BUF_SIZE];
    uint32_t len = 0U;

    if (master->state != MODULE_MODBUS_MASTER_WAIT_RESPONSE) {
        return;
    }

    if (module_modbus_pop_waiting_response(frame, &len)) {
        module_modbus_process_frame(frame, len);
    }
}

void module_modbus_queue_fill_task(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    module_modbus_poll_step_t step;
    const module_modbus_poll_step_map_t *entry;
    bool queued = false;

    if (master->poll_index >= MODULE_MODBUS_POLL_STEP_COUNT) {
        master->poll_index = 0U;
    }

    step = (module_modbus_poll_step_t)master->poll_index;
    master->poll_index++;
    if (master->poll_index >= MODULE_MODBUS_POLL_STEP_COUNT) {
        master->poll_index = 0U;
    }

    if ((uint32_t)step < (sizeof(s_modbus_poll_step_map) /
                          sizeof(s_modbus_poll_step_map[0]))) {
        entry = &s_modbus_poll_step_map[step];
        if ((entry->read != NULL) && (entry->addr != NULL)) {
            queued = entry->read(*entry->addr, entry->start, entry->count);
        }
    }

    if (!queued) {
        MODULE_MODBUS_MASTER_DIAG_INC(master, frame_error_count);
    }
}

static bool module_modbus_send_next_queued(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    uint8_t *tx_buf = modbus_rs485_tx_buffer(&s_modbus.bus);
    modbus_response_expect_t waiting;
    uint16_t frame_len = 0U;

    if (modbus_rs485_tx_busy(&s_modbus.bus)) {
        return false;
    }

    if (tx_buf == NULL) {
        return false;
    }

    if (!modbus_tx_queue_dequeue(tx_buf,
                                 MODULE_MODBUS_TX_BUF_SIZE,
                                 &frame_len,
                                 &waiting)) {
        return false;
    }

    s_modbus.master.waiting_item = waiting;
    module_modbus_clear_rx_buffers();

    if (!modbus_rs485_start_tx_dma(&s_modbus.bus, frame_len)) {
        MODULE_MODBUS_MASTER_DIAG_INC(master, frame_error_count);
        return false;
    }

    module_modbus_start_waiting_response();
    return true;
}

static uint32_t module_modbus_response_timeout_ticks(void)
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

void module_modbus_master_task(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    uint32_t timeout_ticks = module_modbus_response_timeout_ticks();

    master->tick_count++;
    modbus_rs485_tx_service(&s_modbus.bus);
    module_modbus_parse_ringbuf_frames();

    if ((master->state == MODULE_MODBUS_MASTER_WAIT_RESPONSE) &&
        ((master->tick_count - master->waiting_tick) >= timeout_ticks)) {
        module_modbus_mark_device_offline(&master->waiting_item);
        module_modbus_finish_waiting_response();
        MODULE_MODBUS_MASTER_DIAG_INC(master, timeout_count);
    }

    if ((master->state == MODULE_MODBUS_MASTER_IDLE) &&
        !modbus_rs485_tx_busy(&s_modbus.bus)) {
        (void)module_modbus_send_next_queued();
    }
}

void module_modbus_usart2_irq_handler(void)
{
    modbus_rs485_usart_irq_handler(&s_modbus.bus);
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
