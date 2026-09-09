#include "COMM/modbus/devices/modbus_xm8189.h"

#include <stddef.h>

#define XM8189_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm8189_wind_data_t, field_), (min_), (max_), MODBUS_REG_U16 }

static const modbus_reg_map_t xm8189_register_maps[] = {
    XM8189_RANGE_ITEM(0U, speed_centi, 0, XM8189_SPEED_MAX_CENTI_MS),
    XM8189_RANGE_ITEM(1U, direction_centi_deg, 0, XM8189_DIRECTION_MAX_CENTI_DEG),
};

static const modbus_reg_map_t xm8189_calibration_maps[] = {
    XM8189_RANGE_ITEM(0U, speed_calibration_centi_ms, -1000, 1000),
    XM8189_RANGE_ITEM(1U, direction_calibration_centi_deg, -1000, 1000),
};

static bool modbus_xm8189_calib_ok(uint16_t value)
{
    int16_t signed_value = (int16_t)value;

    return (signed_value >= -1000) && (signed_value <= 1000);
}

static bool modbus_xm8189_write_ok(uint16_t reg, uint16_t value)
{
    switch (reg) {
    case XUNCHIP_REG_MODEL:
        return true;

    case XUNCHIP_REG_POINT_COUNT:
        return (value >= 1U) && (value <= 20U);

    case XUNCHIP_REG_COMM_MODE:
        return (value >= 1U) && (value <= 4U);

    case XUNCHIP_REG_PROTOCOL:
        return (value >= 1U) && (value <= 10U);

    case XUNCHIP_REG_CALIBRATION0:
    case (XUNCHIP_REG_CALIBRATION0 + 1U):
        return modbus_xm8189_calib_ok(value);

    default:
        return false;
    }
}

uint32_t modbus_xm8189_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }

    return modbus_rtu_build_read_request(slave_addr, MODBUS_FC_READ_HOLDING_REGS, start_reg, reg_count, frame, capacity);
}

uint32_t modbus_xm8189_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }
    if ((reg == XUNCHIP_REG_DEVICE_ADDR) || (reg == XUNCHIP_REG_BAUDRATE)) {
        return 0U;
    }
    if (!modbus_xm8189_write_ok(reg, value)) {
        return 0U;
    }

    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG, reg, value, frame, capacity);
}

modbus_rtu_status_t modbus_xm8189_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm8189_wind_data_t *out)
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

modbus_rtu_status_t modbus_xm8189_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8189_wind_data_t *out)
{
    return modbus_parse_ranges(frame, len, slave_addr, XM8189_MEASUREMENT_REG_COUNT, out, (uint16_t)offsetof(xm8189_wind_data_t, status),
        xm8189_register_maps, (uint16_t)(sizeof(xm8189_register_maps) / sizeof(xm8189_register_maps[0])));
}

modbus_rtu_status_t modbus_xm8189_parse_calibration(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8189_wind_data_t *out)
{
    modbus_rtu_status_t status;

    status = modbus_parse_ranges(frame, len, slave_addr, XM8189_CALIBRATION_REG_COUNT, out, (uint16_t)offsetof(xm8189_wind_data_t, status),
        xm8189_calibration_maps, (uint16_t)(sizeof(xm8189_calibration_maps) / sizeof(xm8189_calibration_maps[0])));
    if ((status == MODBUS_RTU_OK) && !out->status.sensor_fault) {
        out->calibration_valid = 0x03U;
    }

    return status;
}
