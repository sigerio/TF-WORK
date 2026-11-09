#include "COMM/modbus/modbus_devices.h"

#include <string.h>

#include "APP/app_status.h"
#include "COMM/modbus/modbus_master.h"

/** @brief 设备数据缓存（所有从机的上次成功解析数据） */
typedef struct {
    xm8537_water_data_t xm8537;
    weather_sensor_data_t weather;
    ac_device_t ac;
    mt80_device_t mt80;
    bool machine_extend_online;
    bool pole_raise_online;
    bool solar_unfold_online;
} modbus_data_cache_t;

static modbus_data_cache_t device_cache = {
    .weather = { .slave_addr = MODULE_MODBUS_WEATHER_SENSOR_ADDR },
    .xm8537 = { .slave_addr = MODULE_MODBUS_XM8537_ADDR },
    .ac = { .slave_addr = MODULE_MODBUS_AC_ADDR },
    .mt80 = { .slave_addr = MODULE_MODBUS_MT80_ADDR },
};

typedef enum {
    MODBUS_QUERY_WEATHER_SENSOR = 0,
    MODBUS_QUERY_XM8537,
    MODBUS_QUERY_AC_MEASURE,
    MODBUS_QUERY_AC_HUMIDITY,
    MODBUS_QUERY_AC_CONTROL,
    MODBUS_QUERY_AC_STATUS,
    MODBUS_QUERY_WEATHER_FAULT,
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
static struct {
    uint16_t inputs;
    bool valid;
    wanrong_command_e command;
} wanrong_motors[3];

/* MT80 按补充协议同时读取 L/H，避免跨事务拼接 32 位量。 */
static const struct {
    uint8_t function;
    uint16_t start;
    uint16_t count;
} mt80_poll_plan[] = {
    {MODBUS_FC_READ_INPUT_REGS, 0x3521, 5},
    {MODBUS_FC_READ_INPUT_REGS, 0x3529, 3},
    {MODBUS_FC_READ_INPUT_REGS, 0x352C, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x352E, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3530, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3549, 4},
    {MODBUS_FC_READ_INPUT_REGS, 0x3551, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3553, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3555, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3557, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3559, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x3560, 4},
    {MODBUS_FC_READ_INPUT_REGS, 0x3564, 3},
    {MODBUS_FC_READ_INPUT_REGS, 0x3567, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3569, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x356B, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x356F, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x3580, 3},
    {MODBUS_FC_READ_INPUT_REGS, 0x3586, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x3589, 1},
    {MODBUS_FC_READ_COILS, MT80_COIL_INVERTER_OUTPUT, 1},
    {MODBUS_FC_READ_COILS, MT80_COIL_REMOTE_CONTROL, 1},
    {MODBUS_FC_READ_COILS, MT80_COIL_DC_OUTPUT, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x340A, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x3410, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x3416, 1},
    {MODBUS_FC_READ_INPUT_REGS, 0x3418, 2},
    {MODBUS_FC_READ_INPUT_REGS, 0x341A, 2},
    {MODBUS_FC_READ_HOLDING_REGS, 0x960F, 3},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9613, 16},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9623, 2},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9631, 6},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9656, 4},
    {MODBUS_FC_READ_HOLDING_REGS, 0x965C, 10},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9666, 3},
    {MODBUS_FC_READ_HOLDING_REGS, 0x9669, 3},
    {MODBUS_FC_READ_HOLDING_REGS, 0x966C, 3},
    {MODBUS_FC_READ_HOLDING_REGS, 0x966F, 3},
    {MODBUS_FC_READ_COILS, MT80_COIL_CLEAR_ENERGY, 1},
    {MODBUS_FC_READ_COILS, MT80_COIL_FAULT_RESET, 1},
};

static const modbus_poll_entry_t poll_plan[MODBUS_QUERY_WEATHER_FAULT] = {
    [MODBUS_QUERY_WEATHER_SENSOR] = { modbus_weather_sensor_build_read, &device_cache.weather.slave_addr,
        WEATHER_SENSOR_REG_START, WEATHER_SENSOR_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_WEATHER_SENSOR, },
    [MODBUS_QUERY_XM8537] = { modbus_xm8537_build_read_request, &device_cache.xm8537.slave_addr,
        XUNCHIP_REG_DATA0, XM8537_MEASUREMENT_REG_COUNT, MODBUS_BUS_WEATHER, MODBUS_DEV_XM8537_WATER, },
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
    memset(wanrong_motors, 0, sizeof(wanrong_motors));
    memset(&device_cache.weather, 0, sizeof(device_cache.weather));
    device_cache.weather.slave_addr = MODULE_MODBUS_WEATHER_SENSOR_ADDR;
    memset(&device_cache.xm8537.status, 0, sizeof(device_cache.xm8537.status));
    memset(&device_cache.ac.status, 0, sizeof(device_cache.ac.status));
    memset(&device_cache.mt80, 0, sizeof(device_cache.mt80));
    device_cache.mt80.slave_addr = MODULE_MODBUS_MT80_ADDR;
    device_cache.machine_extend_online = false;
    device_cache.pole_raise_online = false;
    device_cache.solar_unfold_online = false;
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
    case MODBUS_DEV_MACHINE_EXTEND:
        device_cache.machine_extend_online = online;
        return;
    case MODBUS_DEV_POLE_RAISE:
        device_cache.pole_raise_online = online;
        return;
    case MODBUS_DEV_SOLAR_UNFOLD:
        device_cache.solar_unfold_online = online;
        return;
    case MODBUS_DEV_WEATHER_SENSOR:
        device_cache.weather.status.online = online;
        if (online) {
            set_signal(OS_485_TH_ONLINE);
            set_signal(OS_485_WIND_ONLINE);
            set_signal(OS_485_RAIN_ONLINE);
        } else {
            cancel_signal(OS_485_TH_ONLINE);
            cancel_signal(OS_485_WIND_ONLINE);
            cancel_signal(OS_485_RAIN_ONLINE);
        }
        return;
    case MODBUS_DEV_MT80:
        device_cache.mt80.status.online = online;
        return;
    case MODBUS_DEV_XM8537_WATER:
        status = &device_cache.xm8537.status;
        signal = OS_485_WATER_ONLINE;
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

bool modbus_xm8537_get(xm8537_water_data_t *out)
{
    if (out == NULL) {
        return false;
    }
    *out = device_cache.xm8537;
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
    case MODBUS_FC_READ_INPUT_REGS:
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
    case MODBUS_FC_WRITE_MULTIPLE_REGS:
        expect.req_type = MODBUS_REQ_WRITE_REGS;
        expect.quantity = modbus_rtu_get_u16_be(&frame[4]);
        expect.echo_addr = modbus_rtu_get_u16_be(&frame[2]);
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
    const uint32_t mt80_count = sizeof(mt80_poll_plan) / sizeof(mt80_poll_plan[0]);
    const uint32_t step_count = 5U * mt80_count + 5U;
    uint32_t step = poll_step;
    uint32_t query;

    poll_step = (poll_step + 1U) % step_count;
    /* 每组穿插三台电机状态，避免限位读取等到整个 MT80 轮询结束。 */
    if (step < 5U * mt80_count) {
        uint32_t index = step / 5U;
        uint32_t slot = step % 5U;
        if (slot >= 2U) {
            const uint8_t addresses[] = {MODULE_MODBUS_MACHINE_EXTEND_ADDR, MODULE_MODBUS_POLE_RAISE_ADDR,
                MODULE_MODBUS_SOLAR_UNFOLD_ADDR};
            modbus_dev_type_t device = (modbus_dev_type_t)(MODBUS_DEV_MACHINE_EXTEND + slot - 2U);
            len = modbus_wanrong_build_read_status(addresses[slot - 2U], frame, sizeof(frame));
            (void)modbus_devices_enqueue(MODBUS_BUS_WEATHER, device, frame, len, WANRONG_REG_INPUT_STATUS);
            return;
        }
        if (slot == 1U) {
            (void)modbus_mt80_queue_read(mt80_poll_plan[index].function, mt80_poll_plan[index].start, mt80_poll_plan[index].count);
            return;
        }
        query = (index & 1U) ? MODBUS_QUERY_XM8537 : MODBUS_QUERY_WEATHER_SENSOR;
    } else {
        query = MODBUS_QUERY_AC_MEASURE + step - 5U * mt80_count;
    }
    if (query == MODBUS_QUERY_WEATHER_FAULT) {
        modbus_master_repeat_weather_fault();
        return;
    }
    entry = &poll_plan[query];
    len = entry->build_request(*entry->slave_addr, entry->start_addr, entry->quantity, frame, sizeof(frame));
    (void)modbus_devices_enqueue(entry->bus, entry->device, frame, len, entry->start_addr);
}

bool modbus_xm8537_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_xm8537_build_write_request(slave_addr, reg, value, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_XM8537_WATER, frame, len, reg);
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

bool modbus_wanrong_queue_command(modbus_dev_type_t device, wanrong_command_e command)
{
    uint8_t address;
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len;

    switch (device) {
    case MODBUS_DEV_MACHINE_EXTEND: address = MODULE_MODBUS_MACHINE_EXTEND_ADDR; break;
    case MODBUS_DEV_POLE_RAISE: address = MODULE_MODBUS_POLE_RAISE_ADDR; break;
    case MODBUS_DEV_SOLAR_UNFOLD: address = MODULE_MODBUS_SOLAR_UNFOLD_ADDR; break;
    default: return false;
    }
    uint32_t index = (uint32_t)(device - MODBUS_DEV_MACHINE_EXTEND);
    if (command != WANRONG_STOP && wanrong_motors[index].valid) {
        uint16_t inputs = wanrong_motors[index].inputs;
        if ((inputs & (WANRONG_CW_START | WANRONG_CCW_START)) != 0U ||
            (command == WANRONG_FORWARD && (inputs & WANRONG_CW_LIMIT) != 0U) ||
            (command == WANRONG_REVERSE && (inputs & WANRONG_CCW_LIMIT) != 0U)) return false;
    }
    len = modbus_wanrong_build_command(address, command, frame, sizeof(frame));
    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, device, frame, len, WANRONG_REG_COMMAND);
}

void modbus_wanrong_command_sent(const modbus_rsp_expect_t *expect)
{
    if (expect->dev_type < MODBUS_DEV_MACHINE_EXTEND || expect->dev_type > MODBUS_DEV_SOLAR_UNFOLD ||
        expect->req_type != MODBUS_REQ_WRITE_REG || expect->echo_addr != WANRONG_REG_COMMAND ||
        expect->echo_value == WANRONG_STOP || expect->echo_value > WANRONG_REVERSE) return;
    wanrong_motors[expect->dev_type - MODBUS_DEV_MACHINE_EXTEND].command = (wanrong_command_e)expect->echo_value;
}

bool modbus_mt80_queue_read(uint8_t function, uint16_t start, uint16_t count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_mt80_build_read_request(MODULE_MODBUS_MT80_ADDR, function, start, count, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_MT80, frame, len, start);
}

bool modbus_mt80_queue_write_params(uint16_t start, const uint16_t *values, uint16_t count)
{
    uint8_t frame[MT80_WRITE_FRAME_MAX];
    uint32_t len = modbus_mt80_build_write_params(MODULE_MODBUS_MT80_ADDR, start, values, count, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_MT80, frame, len, start);
}

bool modbus_mt80_queue_write_coil(uint16_t coil, bool on)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_mt80_build_write_coil(MODULE_MODBUS_MT80_ADDR, coil, on, frame, sizeof(frame));

    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_MT80, frame, len, coil);
}

bool modbus_mt80_get(mt80_device_t *out)
{
    if (out == NULL) return false;
    *out = device_cache.mt80;
    return true;
}

typedef modbus_rtu_status_t (*modbus_rsp_parse_fn_t)(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache);

static modbus_rtu_status_t modbus_parse_sensor_read(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    if (expect->dev_type == MODBUS_DEV_WEATHER_SENSOR) {
        if (expect->start_addr != WEATHER_SENSOR_REG_START || expect->quantity != WEATHER_SENSOR_REG_COUNT ||
            expect->rsp_func != MODBUS_FC_READ_HOLDING_REGS) return MODBUS_RTU_ERR_PARAM;
        return modbus_weather_sensor_parse(frame, len, expect->slave_addr, &cache->weather);
    }
    return modbus_xm8537_parse_measurement(frame, len, expect->slave_addr, &cache->xm8537);
}

static modbus_rtu_status_t modbus_parse_sensor_write(const uint8_t *frame,
    uint32_t len, const modbus_rsp_expect_t *expect, modbus_data_cache_t *cache)
{
    if (expect->dev_type == MODBUS_DEV_WEATHER_SENSOR) {
        return modbus_rtu_parse_write_single_response(frame, len, expect->slave_addr,
            MODBUS_FC_WRITE_SINGLE_REG, expect->echo_addr, expect->echo_value);
    }
    return modbus_xm8537_parse_write_ack(frame, len, expect->slave_addr,
        expect->rsp_func, expect->echo_addr, expect->echo_value, &cache->xm8537);
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
    [MODBUS_DEV_WEATHER_SENSOR] = {
        [MODBUS_REQ_READ_REGS] = modbus_parse_sensor_read,
        [MODBUS_REQ_WRITE_REG] = modbus_parse_sensor_write,
    },
    [MODBUS_DEV_XM8537_WATER] = {
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

    switch (expect->dev_type) {
    case MODBUS_DEV_MACHINE_EXTEND:
    case MODBUS_DEV_POLE_RAISE:
    case MODBUS_DEV_SOLAR_UNFOLD: {
        uint32_t index = (uint32_t)(expect->dev_type - MODBUS_DEV_MACHINE_EXTEND);
        if (expect->req_type == MODBUS_REQ_READ_REGS) {
            uint16_t inputs;
            bool cw;
            bool ccw;
            if (expect->rsp_func != MODBUS_FC_READ_HOLDING_REGS ||
                expect->start_addr != WANRONG_REG_INPUT_STATUS || expect->quantity != 1U) return MODBUS_RTU_ERR_PARAM;
            status = modbus_wanrong_parse_status(frame, len, expect->slave_addr, &inputs);
            if (status != MODBUS_RTU_OK) return status;
            wanrong_motors[index].inputs = inputs;
            wanrong_motors[index].valid = true;
            if ((inputs & (WANRONG_CW_START | WANRONG_CCW_START)) != 0U) {
                cw = (inputs & WANRONG_CW_START) != 0U;
                ccw = (inputs & WANRONG_CCW_START) != 0U;
                /* 现场按键接管后，旧通讯方向和排队运动不得重新生效。 */
                wanrong_motors[index].command = WANRONG_STOP;
                modbus_txq_drop_wanrong_motion(expect->dev_type);
            } else {
                cw = wanrong_motors[index].command == WANRONG_FORWARD;
                ccw = wanrong_motors[index].command == WANRONG_REVERSE;
            }
            if ((cw && (inputs & WANRONG_CW_LIMIT) != 0U) ||
                (ccw && (inputs & WANRONG_CCW_LIMIT) != 0U)) {
                modbus_master_queue_wanrong_stop(expect->dev_type);
            }
            return MODBUS_RTU_OK;
        }
        if (expect->req_type != MODBUS_REQ_WRITE_REG || expect->rsp_func != MODBUS_FC_WRITE_SINGLE_REG ||
            expect->echo_addr != WANRONG_REG_COMMAND) return MODBUS_RTU_ERR_FUNC;
        status = modbus_wanrong_parse_ack(frame, len, expect->slave_addr, expect->echo_value);
        if (status == MODBUS_RTU_OK && expect->echo_value == WANRONG_STOP) {
            wanrong_motors[index].command = WANRONG_STOP;
        }
        return status;
    }
    case MODBUS_DEV_MT80:
        switch (expect->req_type) {
        case MODBUS_REQ_READ_REGS:
        case MODBUS_REQ_READ_BITS:
            return modbus_mt80_parse_read(frame, len, expect->slave_addr, expect->rsp_func,
                expect->start_addr, expect->quantity, &device_cache.mt80);
        case MODBUS_REQ_WRITE_REGS:
            return modbus_rtu_parse_write_regs_response(frame, len, expect->slave_addr, expect->echo_addr, expect->quantity);
        case MODBUS_REQ_WRITE_COIL:
            return modbus_rtu_parse_write_single_response(frame, len, expect->slave_addr,
                MODBUS_FC_WRITE_SINGLE_COIL, expect->echo_addr, expect->echo_value);
        default: return MODBUS_RTU_ERR_FUNC;
        }
    default: break;
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

bool modbus_weather_sensor_get(weather_sensor_data_t *out)
{
    if (out == NULL) return false;
    *out = device_cache.weather;
    return true;
}

bool modbus_weather_sensor_queue_write(uint16_t reg, uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = modbus_weather_sensor_build_write(MODULE_MODBUS_WEATHER_SENSOR_ADDR, reg, value, frame, sizeof(frame));
    return modbus_devices_enqueue(MODBUS_BUS_WEATHER, MODBUS_DEV_WEATHER_SENSOR, frame, len, reg);
}
