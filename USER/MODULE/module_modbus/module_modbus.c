#include "module_modbus.h"

#include <string.h>

#include "modbus_rs485_bus.h"
#include "modbus_tx_queue.h"
#include "module_modbus_devices.h"
#include "tf_delay.h"
#include "tf_debug.h"

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#ifndef MODULE_MODBUS_POLL_PERIOD_MS
#define MODULE_MODBUS_POLL_PERIOD_MS           MODULE_MODBUS_MASTER_PERIOD_MS
#endif

#ifndef MODULE_MODBUS_RESPONSE_TIMEOUT_MS
#define MODULE_MODBUS_RESPONSE_TIMEOUT_MS      60U
#endif

#ifndef MODULE_MODBUS_POLL_ITEM_MAX
#define MODULE_MODBUS_POLL_ITEM_MAX            10U
#endif

#ifndef MODULE_MODBUS_AC_ADDR
#define MODULE_MODBUS_AC_ADDR                   5U
#endif

typedef struct {
    uint8_t addr;
    module_modbus_device_type_t device;
    module_modbus_poll_function_t function;
    uint16_t start;
    uint16_t count;
    bool enabled;
} module_modbus_poll_item_t;

typedef struct {
    module_modbus_poll_item_t items[MODULE_MODBUS_POLL_ITEM_MAX];
    uint32_t item_count;
    module_modbus_task_status_t status;

    module_modbus_master_state_t state;
    modbus_response_expect_t waiting_item;
    uint32_t waiting_tick;
} module_modbus_master_context_t;

typedef struct {
    xm7828_th_data_t xm7828;
    xm8189_wind_data_t xm8189;
    xm8537_water_data_t xm8537;
    xm9505_rain_data_t xm9505;
    ac_params_t ac_params;
    ac_status_t ac_status;
} module_modbus_device_context_t;

typedef struct {
    modbus_rs485_bus_t bus;
    modbus_tx_queue_t send_queue;
    module_modbus_master_context_t master;
    module_modbus_device_context_t devices;
} module_modbus_context_t;

static module_modbus_context_t s_modbus;

static const module_modbus_poll_item_t s_static_poll_items[] = {
    {1U, MODULE_MODBUS_DEV_XM7828_TH,    MODULE_MODBUS_POLL_READ_REGS,
     XUNCHIP_REG_DATA0, XM7828_MEASUREMENT_REG_COUNT, true},
    {2U, MODULE_MODBUS_DEV_XM8189_WIND,  MODULE_MODBUS_POLL_READ_REGS,
     XUNCHIP_REG_DATA0, XM8189_MEASUREMENT_REG_COUNT, true},
    {3U, MODULE_MODBUS_DEV_XM8537_WATER, MODULE_MODBUS_POLL_READ_REGS,
     XUNCHIP_REG_DATA0, XM8537_MEASUREMENT_REG_COUNT, true},
    {4U, MODULE_MODBUS_DEV_XM9505_RAIN,  MODULE_MODBUS_POLL_READ_REGS,
     XUNCHIP_REG_DATA0, XM9505_MEASUREMENT_REG_COUNT, true},
    {MODULE_MODBUS_AC_ADDR, MODULE_MODBUS_DEV_AC, MODULE_MODBUS_POLL_READ_REGS,
     AC_PARAM_SEG_MEASURE_START, AC_PARAM_SEG_MEASURE_COUNT, true},
    {MODULE_MODBUS_AC_ADDR, MODULE_MODBUS_DEV_AC, MODULE_MODBUS_POLL_READ_REGS,
     AC_PARAM_SEG_HUMIDITY_START, AC_PARAM_SEG_HUMIDITY_COUNT, true},
    {MODULE_MODBUS_AC_ADDR, MODULE_MODBUS_DEV_AC, MODULE_MODBUS_POLL_READ_BITS,
     AC_STATUS_SEG_ALL_START, AC_STATUS_SEG_ALL_COUNT, true},
};

static uint8_t module_modbus_poll_fc(module_modbus_poll_function_t function)
{
    if (function == MODULE_MODBUS_POLL_READ_BITS) {
        return MODBUS_FC_READ_COILS;
    }

    return MODBUS_FC_READ_HOLDING_REGS;
}

static bool module_modbus_response_shape(module_modbus_poll_function_t function,
                                         uint16_t count,
                                         uint8_t *byte_count,
                                         uint16_t *frame_len)
{
    uint32_t bytes;

    if ((byte_count == NULL) || (frame_len == NULL) || (count == 0U)) {
        return false;
    }

    if (function == MODULE_MODBUS_POLL_READ_BITS) {
        bytes = ((uint32_t)count + 7U) / 8U;
    } else {
        bytes = (uint32_t)count * 2U;
    }

    if ((bytes == 0U) || (bytes > 0xFFU) ||
        ((bytes + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
        return false;
    }

    *byte_count = (uint8_t)bytes;
    *frame_len = (uint16_t)(bytes + 5U);
    return true;
}

static void module_modbus_clear_device_validity(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    devices->xm7828.marks.raw = 0U;
    devices->xm7828.faults.raw = 0U;
    devices->xm7828.status.raw = 0U;
    devices->xm8189.marks.raw = 0U;
    devices->xm8189.faults.raw = 0U;
    devices->xm8189.status.raw = 0U;
    devices->xm8537.marks.raw = 0U;
    devices->xm8537.faults.raw = 0U;
    devices->xm8537.status.raw = 0U;
    devices->xm9505.marks.raw = 0U;
    devices->xm9505.faults.raw = 0U;
    devices->xm9505.status.raw = 0U;
    devices->ac_params.marks.raw = 0U;
    devices->ac_status.marks.raw = 0U;
}

static void module_modbus_clear_device_config(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    memset(&devices->xm7828.config, 0, sizeof(devices->xm7828.config));
    memset(&devices->xm8189.config, 0, sizeof(devices->xm8189.config));
    memset(&devices->xm8537.config, 0, sizeof(devices->xm8537.config));
    memset(&devices->xm9505.config, 0, sizeof(devices->xm9505.config));
    memset(&devices->ac_params.config, 0, sizeof(devices->ac_params.config));
    memset(&devices->ac_status.config, 0, sizeof(devices->ac_status.config));
}

static void module_modbus_apply_device_config(const module_modbus_poll_item_t *item)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;
    module_modbus_device_config_t config;

    if (item == NULL) {
        return;
    }

    config.addr = item->addr;
    config.managed = item->enabled;

    switch (item->device) {
    case MODULE_MODBUS_DEV_XM7828_TH:
        devices->xm7828.config.addr = config.addr;
        devices->xm7828.config.managed =
            (devices->xm7828.config.managed || config.managed);
        break;

    case MODULE_MODBUS_DEV_XM8189_WIND:
        devices->xm8189.config.addr = config.addr;
        devices->xm8189.config.managed =
            (devices->xm8189.config.managed || config.managed);
        break;

    case MODULE_MODBUS_DEV_XM8537_WATER:
        devices->xm8537.config.addr = config.addr;
        devices->xm8537.config.managed =
            (devices->xm8537.config.managed || config.managed);
        break;

    case MODULE_MODBUS_DEV_XM9505_RAIN:
        devices->xm9505.config.addr = config.addr;
        devices->xm9505.config.managed =
            (devices->xm9505.config.managed || config.managed);
        break;

    case MODULE_MODBUS_DEV_AC:
        devices->ac_params.config.addr = config.addr;
        devices->ac_params.config.managed =
            (devices->ac_params.config.managed || config.managed);
        devices->ac_status.config.addr = config.addr;
        devices->ac_status.config.managed =
            (devices->ac_status.config.managed || config.managed);
        break;

    case MODULE_MODBUS_DEV_CUSTOM:
    default:
        break;
    }
}

static void module_modbus_sync_device_config_from_poll_items(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;

    /* apply_device_config() OR-accumulates managed; clear first so disabled
       poll items can reset cached device config on each full sync. */
    module_modbus_clear_device_config();
    for (uint32_t i = 0U; i < master->item_count; i++) {
        module_modbus_apply_device_config(&master->items[i]);
    }
}

static void module_modbus_reset_runtime_state(void)
{
    modbus_tx_queue_t *queue = &s_modbus.send_queue;
    module_modbus_master_context_t *master = &s_modbus.master;

    modbus_rs485_reset_runtime(&s_modbus.bus);
    modbus_tx_queue_reset(queue);

    memset(&master->status, 0, sizeof(master->status));
    memset(&master->waiting_item, 0, sizeof(master->waiting_item));
    master->state = MODULE_MODBUS_MASTER_IDLE;
    master->waiting_tick = 0U;

    module_modbus_clear_device_validity();
}

static void module_modbus_load_static_poll_items(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    uint32_t count = (uint32_t)(sizeof(s_static_poll_items) / sizeof(s_static_poll_items[0]));

    if (count > MODULE_MODBUS_POLL_ITEM_MAX) {
        count = MODULE_MODBUS_POLL_ITEM_MAX;
    }

    memcpy(master->items, s_static_poll_items, count * sizeof(master->items[0]));
    master->item_count = count;
    module_modbus_sync_device_config_from_poll_items();
}

static void module_modbus_start_waiting_response(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_response_expect_t *item = &master->waiting_item;

    master->state = MODULE_MODBUS_MASTER_WAIT_RESPONSE;
    master->waiting_tick = master->status.tick_count;
    master->status.waiting_addr = item->addr;
    master->status.waiting_function = item->function;
    master->status.request_count++;
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

static void module_modbus_mark_ac_comm_fault(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    devices->ac_params.marks.raw = 0U;
    devices->ac_status.marks.raw = 0U;
    devices->ac_status.marks.bit.comm_fault = 1U;
    devices->ac_status.comm_fault = true;
}

static void module_modbus_clear_ac_comm_fault(void)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    devices->ac_status.marks.bit.comm_fault = 0U;
    devices->ac_status.comm_fault = false;
}

static void module_modbus_mark_xunchip_comm_fault(const modbus_response_expect_t *item)
{
    module_modbus_device_context_t *devices = &s_modbus.devices;

    if (item == NULL) {
        return;
    }

    switch (item->device) {
    case MODULE_MODBUS_DEV_XM7828_TH:
        devices->xm7828.marks.raw = 0U;
        devices->xm7828.faults.raw = 0U;
        devices->xm7828.status.raw = 0U;
        devices->xm7828.status.bit.comm_fault = 1U;
        break;

    case MODULE_MODBUS_DEV_XM8189_WIND:
        devices->xm8189.marks.raw = 0U;
        devices->xm8189.faults.raw = 0U;
        devices->xm8189.status.raw = 0U;
        devices->xm8189.status.bit.comm_fault = 1U;
        break;

    case MODULE_MODBUS_DEV_XM8537_WATER:
        devices->xm8537.marks.raw = 0U;
        devices->xm8537.faults.raw = 0U;
        devices->xm8537.status.raw = 0U;
        devices->xm8537.status.bit.comm_fault = 1U;
        break;

    case MODULE_MODBUS_DEV_XM9505_RAIN:
        devices->xm9505.marks.raw = 0U;
        devices->xm9505.faults.raw = 0U;
        devices->xm9505.status.raw = 0U;
        devices->xm9505.status.bit.comm_fault = 1U;
        break;

    case MODULE_MODBUS_DEV_AC:
        module_modbus_mark_ac_comm_fault();
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
    module_modbus_load_static_poll_items();
    tf_time_init();

    return true;
}

void module_modbus_set_rx_callback(module_modbus_rx_callback_t callback)
{
    modbus_rs485_set_rx_callback(&s_modbus.bus, callback);
}

void module_modbus_get_task_status(module_modbus_task_status_t *status)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_tx_queue_t *queue = &s_modbus.send_queue;
    bool waiting_response;

    if (status == NULL) {
        return;
    }

    waiting_response = (master->state == MODULE_MODBUS_MASTER_WAIT_RESPONSE);

    *status = master->status;
    status->state = master->state;
    status->tx_busy = modbus_rs485_tx_busy(&s_modbus.bus);
    status->waiting_response = waiting_response;
    status->waiting_addr = waiting_response ? master->waiting_item.addr : 0U;
    status->waiting_function = waiting_response ?
        master->waiting_item.function : MODULE_MODBUS_POLL_READ_REGS;
    status->queue_count = queue->count;
    status->send_buf_write_index = queue->write_index;
    status->enqueue_count = queue->enqueue_count;
    status->dequeue_count = queue->dequeue_count;
    status->queue_overwrite_drop_count = queue->overwrite_drop_count;
    status->queue_full_drop_count = queue->full_drop_count;
    status->queue_invalid_count = queue->invalid_count;
}

bool module_modbus_get_xm7828_data(xm7828_th_data_t *out)
{
    xm7828_th_data_t *cache = &s_modbus.devices.xm7828;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return ((cache->marks.raw != 0U) || (cache->faults.raw != 0U) ||
            (cache->status.raw != 0U));
}

bool module_modbus_get_xm8189_data(xm8189_wind_data_t *out)
{
    xm8189_wind_data_t *cache = &s_modbus.devices.xm8189;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return ((cache->marks.raw != 0U) || (cache->faults.raw != 0U) ||
            (cache->status.raw != 0U));
}

bool module_modbus_get_xm8537_data(xm8537_water_data_t *out)
{
    xm8537_water_data_t *cache = &s_modbus.devices.xm8537;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return ((cache->marks.raw != 0U) || (cache->faults.raw != 0U) ||
            (cache->status.raw != 0U));
}

bool module_modbus_get_xm9505_data(xm9505_rain_data_t *out)
{
    xm9505_rain_data_t *cache = &s_modbus.devices.xm9505;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return ((cache->marks.raw != 0U) || (cache->faults.raw != 0U) ||
            (cache->status.raw != 0U));
}

bool module_modbus_get_ac_params(ac_params_t *out)
{
    ac_params_t *cache = &s_modbus.devices.ac_params;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return (cache->marks.raw != 0U);
}

bool module_modbus_get_ac_status(ac_status_t *out)
{
    ac_status_t *cache = &s_modbus.devices.ac_status;

    if (out == NULL) {
        return false;
    }

    *out = *cache;
    return (cache->marks.raw != 0U);
}

uint32_t module_modbus_available(void)
{
    return modbus_rs485_rx_available(&s_modbus.bus);
}

uint32_t module_modbus_read(uint8_t *buf, uint32_t buf_size)
{
    return modbus_rs485_rx_read(&s_modbus.bus, buf, buf_size);
}

uint32_t module_modbus_drop(uint32_t len)
{
    return modbus_rs485_rx_drop(&s_modbus.bus, len);
}

uint32_t module_modbus_rx_overflow_count(void)
{
    return modbus_rs485_rx_overflow_count(&s_modbus.bus);
}

static bool module_modbus_poll_response_shape(const module_modbus_poll_item_t *item,
                                              uint8_t *byte_count,
                                              uint32_t *frame_len)
{
    uint16_t len;

    if ((item == NULL) || (frame_len == NULL)) {
        return false;
    }

    if (!module_modbus_response_shape(item->function, item->count, byte_count, &len)) {
        return false;
    }

    *frame_len = len;
    return true;
}

static bool module_modbus_pop_waiting_response(uint8_t *frame, uint32_t *frame_len)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    module_modbus_task_status_t *status = &master->status;
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

    if ((item->addr == 0U) || (item->expected_len == 0U) ||
        (item->expected_len > MODULE_MODBUS_RX_BUF_SIZE)) {
        module_modbus_mark_xunchip_comm_fault(item);
        status->frame_error_count++;
        module_modbus_finish_waiting_response();
        return false;
    }

    expect_function = item->expected_func;
    expect_byte_count = item->expected_bytes;
    expected_len = item->expected_len;
    scan_budget = module_modbus_available();

    while (scan_budget >= 2U) {
        available = module_modbus_available();
        if (available < 2U) {
            return false;
        }

        if (module_modbus_peek_rx(head, 2U) != 2U) {
            return false;
        }

        exception_response = (head[1] == (uint8_t)(expect_function | 0x80U));
        if ((head[0] != item->addr) ||
            ((head[1] != expect_function) && !exception_response)) {
            if (module_modbus_drop(1U) != 1U) {
                return false;
            }
            status->unmatched_count++;
            scan_budget--;
            continue;
        }

        if (exception_response) {
            expected_len = MODBUS_RTU_EXCEPTION_LEN;
            available = module_modbus_available();
            if (available < expected_len) {
                return false;
            }

            if (module_modbus_peek_rx(frame, expected_len) != expected_len) {
                return false;
            }

            (void)module_modbus_drop(expected_len);
            *frame_len = expected_len;
            return true;
        }

        available = module_modbus_available();
        if (available < 3U) {
            return false;
        }

        if (module_modbus_peek_rx(head, 3U) != 3U) {
            return false;
        }

        if (head[2] != expect_byte_count) {
            (void)module_modbus_drop(3U);
            module_modbus_mark_xunchip_comm_fault(item);
            status->frame_error_count++;
            module_modbus_finish_waiting_response();
            return false;
        }

        available = module_modbus_available();
        if (available < expected_len) {
            return false;
        }

        if (module_modbus_peek_rx(frame, expected_len) != expected_len) {
            return false;
        }

        (void)module_modbus_drop(expected_len);
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
                                                 &devices->ac_status);
        }
        return module_modbus_ac_parse_params(frame,
                                             len,
                                             item->addr,
                                             item->start,
                                             item->count,
                                             &devices->ac_params);

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
    module_modbus_task_status_t *task_status = &s_modbus.master.status;

    if (status == MODBUS_RTU_ERR_CRC) {
        task_status->crc_error_count++;
    } else {
        task_status->parse_error_count++;
    }
}

static void module_modbus_process_frame(const uint8_t *frame, uint32_t len)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    module_modbus_task_status_t *task_status = &master->status;
    modbus_response_expect_t *item = &master->waiting_item;
    uint8_t expect_function;
    modbus_rtu_status_t status;
    module_modbus_rx_callback_t callback = modbus_rs485_get_rx_callback(&s_modbus.bus);

    if (callback != NULL) {
        callback(frame, len);
    }

    expect_function = item->expected_func;
    task_status->response_count++;
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
        module_modbus_mark_xunchip_comm_fault(item);
        module_modbus_finish_waiting_response();
        return;
    }

    status = module_modbus_parse_waiting_response(frame, len);
    if (status == MODBUS_RTU_OK) {
        if (item->device == MODULE_MODBUS_DEV_AC) {
            module_modbus_clear_ac_comm_fault();
        }
        task_status->parse_ok_count++;
    } else {
        module_modbus_mark_xunchip_comm_fault(item);
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

static bool module_modbus_enqueue_poll_item(const module_modbus_poll_item_t *item)
{
    modbus_tx_queue_t *queue = &s_modbus.send_queue;
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint8_t expect_byte_count;
    modbus_response_expect_t waiting;
    uint16_t start = 0U;
    uint32_t len;
    uint32_t expect_frame_len;

    if ((item == NULL) || !item->enabled || (item->addr == 0U) || (item->count == 0U)) {
        return false;
    }

    if (!module_modbus_poll_response_shape(item, &expect_byte_count, &expect_frame_len)) {
        return false;
    }

    if (item->device == MODULE_MODBUS_DEV_AC) {
        uint16_t base = (item->function == MODULE_MODBUS_POLL_READ_BITS) ?
            AC_STATUS_READ_BASE_REF : AC_PARAM_READ_BASE_REF;

        if (item->start < base) {
            return false;
        }
        start = (uint16_t)(item->start - base);
    } else {
        start = item->start;
    }

    if (item->function == MODULE_MODBUS_POLL_READ_BITS) {
        len = module_modbus_build_read_bits(item->addr,
                                            start,
                                            item->count,
                                            frame,
                                            sizeof(frame));
    } else {
        len = module_modbus_build_read_regs(item->addr,
                                            start,
                                            item->count,
                                            frame,
                                            sizeof(frame));
    }

    if (len == 0U) {
        return false;
    }

    memset(&waiting, 0, sizeof(waiting));
    waiting.addr = item->addr;
    waiting.device = item->device;
    waiting.function = item->function;
    waiting.start = item->start;
    waiting.count = item->count;
    waiting.expected_func = module_modbus_poll_fc(item->function);
    waiting.expected_bytes = expect_byte_count;
    waiting.expected_len = (uint16_t)expect_frame_len;

    return modbus_tx_queue_push(queue, frame, (uint16_t)len, &waiting);
}

void module_modbus_queue_fill_task(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    uint32_t checked = 0U;

    if ((master->item_count == 0U) || (master->item_count > MODULE_MODBUS_POLL_ITEM_MAX)) {
        return;
    }

    if (master->status.poll_index >= master->item_count) {
        master->status.poll_index = 0U;
    }

    while (checked < master->item_count) {
        uint8_t index = master->status.poll_index;
        module_modbus_poll_item_t *item = &master->items[index];

        master->status.poll_index++;
        if (master->status.poll_index >= master->item_count) {
            master->status.poll_index = 0U;
        }
        checked++;

        if (!item->enabled) {
            continue;
        }

        if (!module_modbus_enqueue_poll_item(item)) {
            master->status.frame_error_count++;
        }
        break;
    }
}

static bool module_modbus_copy_queue_frame_to_dma_buf(const modbus_tx_queue_item_t *item)
{
    modbus_tx_queue_t *queue = &s_modbus.send_queue;
    uint8_t *tx_buf = modbus_rs485_tx_buffer(&s_modbus.bus);
    uint32_t end_index;

    if ((item == NULL) || (tx_buf == NULL) || (item->len == 0U) ||
        (item->len > MODULE_MODBUS_TX_BUF_SIZE)) {
        queue->invalid_count++;
        return false;
    }

    end_index = (uint32_t)item->buf_index + item->len;
    if (end_index > MODULE_MODBUS_SEND_BUF_SIZE) {
        queue->invalid_count++;
        return false;
    }

    memcpy(tx_buf, &queue->storage[item->buf_index], item->len);
    return true;
}

static void module_modbus_load_waiting_item_from_queue_item(
    const modbus_tx_queue_item_t *item)
{
    if (item == NULL) {
        return;
    }

    s_modbus.master.waiting_item = item->response;
}

static bool module_modbus_send_next_queued(void)
{
    module_modbus_master_context_t *master = &s_modbus.master;
    modbus_tx_queue_t *queue = &s_modbus.send_queue;
    modbus_tx_queue_item_t item;

    if (modbus_rs485_tx_busy(&s_modbus.bus)) {
        return false;
    }

    if (!modbus_tx_queue_peek(queue, &item)) {
        return false;
    }

    if (!module_modbus_copy_queue_frame_to_dma_buf(&item)) {
        modbus_tx_queue_pop(queue);
        master->status.frame_error_count++;
        return false;
    }

    module_modbus_load_waiting_item_from_queue_item(&item);
    module_modbus_clear_rx_buffers();

    if (!modbus_rs485_start_tx_dma(&s_modbus.bus, item.len)) {
        return false;
    }

    modbus_tx_queue_pop(queue);
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

    master->status.tick_count++;
    modbus_rs485_tx_service(&s_modbus.bus);
    module_modbus_parse_ringbuf_frames();

    if ((master->state == MODULE_MODBUS_MASTER_WAIT_RESPONSE) &&
        ((master->status.tick_count - master->waiting_tick) >= timeout_ticks)) {
        module_modbus_mark_xunchip_comm_fault(&master->waiting_item);
        module_modbus_finish_waiting_response();
        master->status.timeout_count++;
    }

    if ((master->state == MODULE_MODBUS_MASTER_IDLE) &&
        !modbus_rs485_tx_busy(&s_modbus.bus)) {
        (void)module_modbus_send_next_queued();
    }
}

bool module_modbus_send(const uint8_t *data, uint32_t len)
{
    return modbus_rs485_send(&s_modbus.bus, data, len);
}

void module_modbus_usart2_irq_handler(void)
{
    modbus_rs485_usart_irq_handler(&s_modbus.bus);
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
