#include "COMM/modbus/devices/modbus_xm8537.h"

#include <stddef.h>

#define XM8537_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm8537_water_data_t, field_), (min_), (max_), MODBUS_REG_U16 }

static const modbus_reg_map_t xm8537_register_maps[] = {
    XM8537_RANGE_ITEM(0U, raw, XM8537_WATER_STATE_NORMAL, XM8537_WATER_STATE_DETECTED),
};

static bool modbus_xm8537_write_ok(uint16_t reg, uint16_t value)
{
    switch (reg) {
    case XUNCHIP_REG_COMM_MODE:
        return (value == 1U) || (value == 2U);

    case XUNCHIP_REG_PROTOCOL:
        return (value == 1U);

    default:
        return false;
    }
}

uint32_t modbus_xm8537_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }

    return modbus_rtu_build_read_request(slave_addr, MODBUS_FC_READ_HOLDING_REGS, start_reg, reg_count, frame, capacity);
}

uint32_t modbus_xm8537_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U) {
        return 0U;
    }
    if ((reg == XUNCHIP_REG_DEVICE_ADDR) || (reg == XUNCHIP_REG_BAUDRATE)) {
        return 0U;
    }
    if (!modbus_xm8537_write_ok(reg, value)) {
        return 0U;
    }

    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG, reg, value, frame, capacity);
}

modbus_rtu_status_t modbus_xm8537_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm8537_water_data_t *out)
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
        /* 配置写入成功不代表测量恢复；故障由后续合法测量值清除。 */
    }

    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_xm8537_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8537_water_data_t *out)
{
    modbus_rtu_status_t status;

    status = modbus_parse_ranges(frame, len, slave_addr, XM8537_MEASUREMENT_REG_COUNT, out, (uint16_t)offsetof(xm8537_water_data_t, status),
        xm8537_register_maps, (uint16_t)(sizeof(xm8537_register_maps) / sizeof(xm8537_register_maps[0])));
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    if (!out->status.sensor_fault) {
        out->water_detected = (out->raw == XM8537_WATER_STATE_DETECTED);
    }

    return MODBUS_RTU_OK;
}
