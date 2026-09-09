#include "COMM/modbus/modbus_devices.h"

#include <string.h>

#include "APP/app_status.h"

/** @brief 设备数据缓存（所有从机的上次成功解析数据） */
typedef struct {
    xm7828_th_data_t xm7828;
    xm8189_wind_data_t xm8189;
    xm8537_water_data_t xm8537;
    xm9505_rain_data_t xm9505;
    ac_device_t ac;
} modbus_data_cache_t;

static modbus_data_cache_t device_cache = {
    .xm7828 = { .slave_addr = MODULE_MODBUS_XM7828_ADDR },
    .xm8189 = { .slave_addr = MODULE_MODBUS_XM8189_ADDR },
    .xm8537 = { .slave_addr = MODULE_MODBUS_XM8537_ADDR },
    .xm9505 = { .slave_addr = MODULE_MODBUS_XM9505_ADDR },
    .ac = { .slave_addr = MODULE_MODBUS_AC_ADDR },
};

typedef enum {
    MODBUS_QUERY_XM7828 = 0,
    MODBUS_QUERY_XM8189,
    MODBUS_QUERY_XM8537,
    MODBUS_QUERY_XM9505,
    MODBUS_QUERY_XM7828_CALIBRATION,
    MODBUS_QUERY_XM8189_CALIBRATION,
    MODBUS_QUERY_AC_MEASURE,
    MODBUS_QUERY_AC_HUMIDITY,
    MODBUS_QUERY_AC_CONTROL,
    MODBUS_QUERY_AC_STATUS,
    MODBUS_QUERY_STEP_COUNT,
} modbus_poll_step_e;

typedef uint32_t (*modbus_build_request_fn_t)(uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint8_t *frame, uint32_t capacity);

typedef struct {
    modbus_build_request_fn_t build_request;
    const uint8_t *slave_addr;
    uint16_t start_addr;
    uint16_t quantity;
    modbus_bus_t bus;
    modbus_dev_type_t device;
} modbus_poll_entry_t;

static uint32_t poll_step;

static const modbus_poll_entry_t poll_plan[MODBUS_QUERY_STEP_COUNT] = {
    [MODBUS_QUERY_XM7828] = { modbus_xm7828_build_read_request, &device_cache.xm7828.slave_addr,
        XUNCHIP_REG_DATA0, XM7828_MEASUREMENT_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM7828_TH, },
    [MODBUS_QUERY_XM8189] = { modbus_xm8189_build_read_request, &device_cache.xm8189.slave_addr,
        XUNCHIP_REG_DATA0, XM8189_MEASUREMENT_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM8189_WIND, },
    [MODBUS_QUERY_XM8537] = { modbus_xm8537_build_read_request, &device_cache.xm8537.slave_addr,
        XUNCHIP_REG_DATA0, XM8537_MEASUREMENT_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM8537_WATER, },
    [MODBUS_QUERY_XM9505] = { modbus_xm9505_build_read_request, &device_cache.xm9505.slave_addr,
        XUNCHIP_REG_DATA0, XM9505_MEASUREMENT_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM9505_RAIN, },
    [MODBUS_QUERY_XM7828_CALIBRATION] = { modbus_xm7828_build_read_request, &device_cache.xm7828.slave_addr,
        XM7828_REG_TEMP_CALIBRATION, XM7828_CALIBRATION_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM7828_TH, },
    [MODBUS_QUERY_XM8189_CALIBRATION] = { modbus_xm8189_build_read_request, &device_cache.xm8189.slave_addr,
        XM8189_REG_SPEED_CALIBRATION, XM8189_CALIBRATION_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM8189_WIND, },
    [MODBUS_QUERY_AC_MEASURE] = { modbus_ac_build_read_params_request, &device_cache.ac.slave_addr,
        AC_PARAM_SEG_MEASURE_START, AC_PARAM_SEG_MEASURE_COUNT, MODBUS_BUS_AC, MODBUS_DEV_AC, },
    [MODBUS_QUERY_AC_HUMIDITY] = { modbus_ac_build_read_params_request, &device_cache.ac.slave_addr,
        AC_PARAM_SEG_HUMIDITY_START, AC_PARAM_SEG_HUMIDITY_COUNT, MODBUS_BUS_AC, MODBUS_DEV_AC, },
    [MODBUS_QUERY_AC_CONTROL] = { modbus_ac_build_read_params_request, &device_cache.ac.slave_addr,
        AC_PARAM_SEG_CONTROL_START, AC_PARAM_SEG_CONTROL_COUNT, MODBUS_BUS_AC, MODBUS_DEV_AC, },
    [MODBUS_QUERY_AC_STATUS] = { modbus_ac_build_read_status_request, &device_cache.ac.slave_addr,
        AC_STATUS_SEG_ALL_START, AC_STATUS_SEG_ALL_COUNT, MODBUS_BUS_AC, MODBUS_DEV_AC, },
};

void modbus_devices_init(void)
{
    poll_step = 0U;
    memset(&device_cache.xm7828.status, 0, sizeof(device_cache.xm7828.status));
    memset(&device_cache.xm8189.status, 0, sizeof(device_cache.xm8189.status));
    memset(&device_cache.xm8537.status, 0, sizeof(device_cache.xm8537.status));
    memset(&device_cache.xm9505.status, 0, sizeof(device_cache.xm9505.status));
    memset(&device_cache.ac.status, 0, sizeof(device_cache.ac.status));
    cancel_signal(OS_485_TH_ONLINE);
    cancel_signal(OS_485_WIND_ONLINE);
    cancel_signal(OS_485_WATER_ONLINE);
    cancel_signal(OS_485_RAIN_ONLINE);
    cancel_signal(OS_485_AC_ONLINE);
}

void modbus_devices_set_online(modbus_dev_type_t device, bool online)
{
    modbus_dev_status_t *status;
    OS_RUN_SIGNAL_EN signal;

    switch (device) {
    case MODBUS_DEV_XM7828_TH:
        status = &device_cache.xm7828.status;
        signal = OS_485_TH_ONLINE;
        break;
    case MODBUS_DEV_XM8189_WIND:
        status = &device_cache.xm8189.status;
        signal = OS_485_WIND_ONLINE;
        break;
    case MODBUS_DEV_XM8537_WATER:
        status = &device_cache.xm8537.status;
        signal = OS_485_WATER_ONLINE;
        break;
    case MODBUS_DEV_XM9505_RAIN:
        status = &device_cache.xm9505.status;
        signal = OS_485_RAIN_ONLINE;
        break;
    case MODBUS_DEV_AC:
        status = &device_cache.ac.status;
        signal = OS_485_AC_ONLINE;
        break;
    default:
        return;
    }

    status->online = online;
    if (online) {
        set_signal(signal);
    } else {
        status->sensor_fault = false;
        cancel_signal(signal);
    }
}

bool modbus_xm7828_get(xm7828_th_data_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.xm7828;
    return true;
}

bool modbus_xm8189_get(xm8189_wind_data_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.xm8189;
    return true;
}

bool modbus_xm8537_get(xm8537_water_data_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.xm8537;
    return true;
}

bool modbus_xm9505_get(xm9505_rain_data_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.xm9505;
    return true;
}

bool modbus_ac_get_params(ac_params_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.ac.params;
    return out->marks.raw != 0U;
}

bool modbus_ac_get_status(ac_status_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.ac.run_status;
    return out->marks.raw != 0U;
}

bool modbus_ac_get(ac_device_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.ac;
    return out->status.online;
}

/* 协议层已完成校验和组帧，这里附加路由与响应匹配信息。 */
static bool modbus_devices_enqueue(modbus_bus_t bus, modbus_dev_type_t device, const uint8_t *frame, uint32_t len, uint16_t start_addr)
{
    modbus_rsp_expect_t expect;

    if (len == 0U) {
        return false;
    }
    memset(&expect, 0, sizeof(expect));
    expect.bus = bus;
    expect.dev_type = device;
    expect.slave_addr = frame[0];
    expect.rsp_func = frame[1];
    expect.start_addr = start_addr;
    switch (frame[1]) {
    case MODBUS_FC_READ_HOLDING_REGS:
        expect.req_type = MODBUS_REQ_READ_REGS;
        expect.quantity = modbus_rtu_get_u16_be(&frame[4]);
        expect.rsp_data_bytes = (uint8_t)(expect.quantity * 2U);
        break;
    case MODBUS_FC_READ_COILS:
        expect.req_type = MODBUS_REQ_READ_BITS;
        expect.quantity = modbus_rtu_get_u16_be(&frame[4]);
        expect.rsp_data_bytes = (uint8_t)(((uint32_t)expect.quantity + 7U) / 8U);
        break;
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_SINGLE_REG:
        expect.req_type = (frame[1] == MODBUS_FC_WRITE_SINGLE_COIL) ? MODBUS_REQ_WRITE_COIL : MODBUS_REQ_WRITE_REG;
        expect.quantity = 1U;
        expect.echo_addr = modbus_rtu_get_u16_be(&frame[2]);
        expect.echo_value = modbus_rtu_get_u16_be(&frame[4]);
        break;
    default:
        return false;
    }
    return modbus_txq_push(frame, (uint16_t)len, &expect);
}

void modbus_devices_poll(void)
{
    const modbus_poll_entry_t *entry;
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len;

    if (poll_step >= MODBUS_QUERY_STEP_COUNT) {
        poll_step = 0U;
    }
    entry = &poll_plan[poll_step];
    poll_step = (poll_step + 1U) % MODBUS_QUERY_STEP_COUNT;
    len = entry->build_request(*entry->slave_addr, entry->start_addr, entry->quantity, frame, sizeof(frame));
    (void)modbus_devices_enqueue(entry->bus, entry->device, frame, len, entry->start_addr);
}

bool modbus_xm7828_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_xm7828_build_write_request(slave_addr, reg, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_XM7828_TH, frame, len, reg);
}

bool modbus_xm8189_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_xm8189_build_write_request(slave_addr, reg, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_XM8189_WIND, frame, len, reg);
}

bool modbus_xm8537_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_xm8537_build_write_request(slave_addr, reg, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_XM8537_WATER, frame, len, reg);
}

bool modbus_xm9505_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_xm9505_build_write_request(slave_addr, reg, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_XM9505_RAIN, frame, len, reg);
}

bool modbus_ac_queue_write_status(uint8_t slave_addr, uint16_t status_ref, bool on)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_ac_build_write_status_request(slave_addr, status_ref, on, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_AC, MODBUS_DEV_AC, frame, len, status_ref);
}

bool modbus_ac_queue_write_param(uint8_t slave_addr, uint16_t param_ref, int16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_ac_build_write_param_request(slave_addr, param_ref, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_AC, MODBUS_DEV_AC, frame, len, param_ref);
}

static void modbus_ac_update_fault(void)
{
    ac_status_t *status = &device_cache.ac.run_status;

    device_cache.ac.status.sensor_fault = (status->cabinet_sensor_fault || status->evaporator_sensor_fault ||
        status->condenser_sensor_fault || status->humidity_sensor_fault);
}

typedef modbus_rtu_status_t (*modbus_rsp_parse_fn_t)(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache);

static modbus_rtu_status_t modbus_parse_sensor_read(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case MODBUS_DEV_XM7828_TH:
        if ((expect->start_addr == XM7828_REG_TEMP_CALIBRATION) && (expect->quantity == XM7828_CALIBRATION_REG_COUNT)) {
            return modbus_xm7828_parse_calibration(frame, len, expect->slave_addr, &cache->xm7828);
        }
        return modbus_xm7828_parse_measurement(frame, len, expect->slave_addr, &cache->xm7828);
    case MODBUS_DEV_XM8189_WIND:
        if ((expect->start_addr == XM8189_REG_SPEED_CALIBRATION) && (expect->quantity == XM8189_CALIBRATION_REG_COUNT)) {
            return modbus_xm8189_parse_calibration(frame, len, expect->slave_addr, &cache->xm8189);
        }
        return modbus_xm8189_parse_measurement(frame, len, expect->slave_addr, &cache->xm8189);
    case MODBUS_DEV_XM8537_WATER:
        return modbus_xm8537_parse_measurement(frame, len, expect->slave_addr, &cache->xm8537);
    case MODBUS_DEV_XM9505_RAIN:
        return modbus_xm9505_parse_measurement(frame, len, expect->slave_addr, &cache->xm9505);
    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t modbus_parse_sensor_write(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    switch (expect->dev_type) {
    case MODBUS_DEV_XM7828_TH:
        return modbus_xm7828_parse_write_ack(frame, len, expect->slave_addr,
            expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->xm7828);
    case MODBUS_DEV_XM8189_WIND:
        return modbus_xm8189_parse_write_ack(frame, len, expect->slave_addr,
            expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->xm8189);
    case MODBUS_DEV_XM8537_WATER:
        return modbus_xm8537_parse_write_ack(frame, len, expect->slave_addr,
            expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->xm8537);
    case MODBUS_DEV_XM9505_RAIN:
        return modbus_xm9505_parse_write_ack(frame, len, expect->slave_addr,
            expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->xm9505);
    default:
        return MODBUS_RTU_ERR_FUNC;
    }
}

static modbus_rtu_status_t modbus_parse_ac_registers(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    return modbus_ac_parse_params(frame, len, expect->slave_addr, expect->start_addr, expect->quantity, &cache->ac.params);
}

static modbus_rtu_status_t modbus_parse_ac_bits(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    return modbus_ac_parse_status(frame, len, expect->slave_addr, expect->start_addr, &cache->ac.run_status);
}

static modbus_rtu_status_t modbus_parse_ac_write(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    return modbus_ac_parse_write_ack(frame, len, expect->slave_addr, expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->ac);
}

static const modbus_rsp_parse_fn_t response_parsers[MODBUS_DEV_COUNT][MODBUS_REQ_COUNT] = {
    [MODBUS_DEV_XM7828_TH] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_sensor_read,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_sensor_write,
    },
    [MODBUS_DEV_XM8189_WIND] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_sensor_read,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_sensor_write,
    },
    [MODBUS_DEV_XM8537_WATER] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_sensor_read,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_sensor_write,
    },
    [MODBUS_DEV_XM9505_RAIN] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_sensor_read,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_sensor_write,
    },
    [MODBUS_DEV_AC] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_ac_registers,
        [MODBUS_REQ_READ_BITS] = modbus_parse_ac_bits,
        [MODBUS_REQ_WRITE_COIL] = modbus_parse_ac_write,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_ac_write,
    },
};

modbus_rtu_status_t modbus_devices_parse_response(const modbus_rsp_expect_t *expect, const uint8_t *frame, uint32_t len)
{
    modbus_rsp_parse_fn_t parse;
    modbus_rtu_status_t status;

    if (((uint32_t)expect->dev_type >= (uint32_t)MODBUS_DEV_COUNT) || ((uint32_t)expect->req_type >= (uint32_t)MODBUS_REQ_COUNT)) {
        return MODBUS_RTU_ERR_FUNC;
    }

    parse = response_parsers[expect->dev_type][expect->req_type];
    if (parse == NULL) {
        return MODBUS_RTU_ERR_FUNC;
    }

    status = parse(frame, len, expect, &device_cache);
    if ((status == MODBUS_RTU_OK) && (expect->dev_type == MODBUS_DEV_AC) && (expect->req_type == MODBUS_REQ_READ_BITS)) {
        modbus_ac_update_fault();
    }
    return status;
}
