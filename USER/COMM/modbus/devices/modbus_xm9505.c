#include "COMM/modbus/devices/modbus_xm9505.h"

#include <stddef.h>

#define XM9505_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm9505_rain_data_t, field_), (min_), (max_), MODBUS_REG_U16 }

static const modbus_reg_map_t xm9505_register_maps[] = {
    XM9505_RANGE_ITEM(0U, rain_count, 0, 0xFFFF),
    XM9505_RANGE_ITEM(1U, instant_tenth_mm, 0, XM9505_RAIN_INSTANT_MAX_TENTH_MM),
    XM9505_RANGE_ITEM(2U, diff, 0, XM9505_DIFF_MAX),
    XM9505_RANGE_ITEM(3U, baseline, 0, XM9505_BASELINE_MAX),
    XM9505_RANGE_ITEM(4U, raindrop_dynamic, 0, XM9505_RAINDROP_DYNAMIC_MAX),
    XM9505_RANGE_ITEM(5U, water_state, 0, XM9505_WATER_STATE_MAX),
};

static bool modbus_xm9505_write_ok(uint16_t reg, uint16_t value)
{
    switch (reg) {
    case XUNCHIP_REG_COMM_MODE:
        return (value == 1U) || (value == 2U);

    case XUNCHIP_REG_PROTOCOL:
        return (value == 1U);

    case (XUNCHIP_REG_PROTOCOL + 1U):
        return (value >= 1U) && (value <= 36000U);

    case XUNCHIP_REG_CALIBRATION0:
        return value <= 100U;

    case (XUNCHIP_REG_CALIBRATION0 + 1U):
        return (value >= 100U) && (value <= 3000U);

    case (XUNCHIP_REG_CALIBRATION0 + 2U):
    case (XUNCHIP_REG_CALIBRATION0 + 3U):
    case (XUNCHIP_REG_CALIBRATION0 + 4U):
        return value == 0U;

    default:
        return false;
    }
}

uint32_t modbus_xm9505_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }

    return modbus_rtu_build_read_request(slave_addr, MODBUS_FC_READ_HOLDING_REGS, start_reg, reg_count, frame, capacity);
}

uint32_t modbus_xm9505_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }
    if ((reg == XUNCHIP_REG_DEVICE_ADDR) || (reg == XUNCHIP_REG_BAUDRATE)) {
        return 0U;
    }
    if (!modbus_xm9505_write_ok(reg, value)) {
        return 0U;
    }

    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG, reg, value, frame, capacity);
}

modbus_rtu_status_t modbus_xm9505_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm9505_rain_data_t *out)
{
    modbus_rtu_status_t status;

    if (function != MODBUS_FC_WRITE_SINGLE_REG) {
        return MODBUS_RTU_ERR_FUNC;
    }

    status = modbus_rtu_parse_write_single_response(frame, len, slave_addr, function, reg, value);
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    if (out != NULL) {
        out->slave_addr = slave_addr;
        out->status.online = true;
        out->status.sensor_fault = false;
    }

    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_xm9505_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm9505_rain_data_t *out)
{
    return modbus_parse_ranges(frame, len, slave_addr, XM9505_MEASUREMENT_REG_COUNT, out, (uint16_t)offsetof(xm9505_rain_data_t, status),
        xm9505_register_maps, (uint16_t)(sizeof(xm9505_register_maps) / sizeof(xm9505_register_maps[0])));
}
