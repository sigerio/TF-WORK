#include "tf_modbus_xm9505.h"

#include <stddef.h>
#include <string.h>

#include "tf_modbus_txq.h"

#define XM9505_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm9505_rain_data_t, field_), \
      (min_), (max_), TF_MODBUS_REG_U16 }

static const tf_modbus_reg_map_t xm9505_register_maps[] = {
    XM9505_RANGE_ITEM(0U, rain_count, 0, 0xFFFF),
    XM9505_RANGE_ITEM(1U, instant_tenth_mm,
                      0,
                      XM9505_RAIN_INSTANT_MAX_TENTH_MM),
    XM9505_RANGE_ITEM(2U, diff, 0, XM9505_DIFF_MAX),
    XM9505_RANGE_ITEM(3U, baseline, 0, XM9505_BASELINE_MAX),
    XM9505_RANGE_ITEM(4U, raindrop_dynamic,
                      0,
                      XM9505_RAINDROP_DYNAMIC_MAX),
    XM9505_RANGE_ITEM(5U, water_state, 0, XM9505_WATER_STATE_MAX),
};

static bool tf_modbus_xm9505_write_ok(uint16_t reg, uint16_t value)
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

bool tf_modbus_xm9505_read(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    tf_modbus_rsp_expect_t expect;
    uint32_t len;

    if (slave_addr == 0U) {
        return false;
    }

    len = modbus_rtu_build_read_request(slave_addr,
                                        MODBUS_FC_READ_HOLDING_REGS,
                                        start_reg,
                                        reg_count,
                                        frame,
                                        sizeof(frame));
    if (len == 0U) {
        return false;
    }

    memset(&expect, 0, sizeof(expect));
    expect.bus = TF_MODBUS_BUS_WEATHER;
    expect.slave_addr = slave_addr;
    expect.dev_type = TF_MODBUS_DEV_XM9505_RAIN;
    expect.read_type = TF_MODBUS_READ_REGS;
    expect.req_type = TF_MODBUS_REQ_READ_REGS;
    expect.start_addr = start_reg;
    expect.quantity = reg_count;
    expect.rsp_func = MODBUS_FC_READ_HOLDING_REGS;
    expect.rsp_data_bytes = (uint8_t)(reg_count * 2U);

    return tf_modbus_txq_push(frame, (uint16_t)len, &expect);
}

bool tf_modbus_xm9505_write(uint8_t slave_addr, uint16_t reg,
                            uint16_t value)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    tf_modbus_rsp_expect_t expect;
    uint32_t len;

    if (slave_addr == 0U) {
        return false;
    }
    if ((reg == XUNCHIP_REG_DEVICE_ADDR) || (reg == XUNCHIP_REG_BAUDRATE)) {
        return false;
    }
    if (!tf_modbus_xm9505_write_ok(reg, value)) {
        return false;
    }

    len = modbus_rtu_build_write_single_request(slave_addr,
                                                MODBUS_FC_WRITE_SINGLE_REG,
                                                reg,
                                                value,
                                                frame,
                                                sizeof(frame));
    if (len == 0U) {
        return false;
    }

    memset(&expect, 0, sizeof(expect));
    expect.bus = TF_MODBUS_BUS_WEATHER;
    expect.slave_addr = slave_addr;
    expect.dev_type = TF_MODBUS_DEV_XM9505_RAIN;
    expect.req_type = TF_MODBUS_REQ_WRITE_REG;
    expect.start_addr = reg;
    expect.quantity = 1U;
    expect.rsp_func = MODBUS_FC_WRITE_SINGLE_REG;
    expect.echo_addr = reg;
    expect.echo_value = value;

    return tf_modbus_txq_push(frame, (uint16_t)len, &expect);
}

modbus_rtu_status_t tf_modbus_xm9505_parse_write_ack(const uint8_t *frame,
                                                     uint32_t len,
                                                     uint8_t slave_addr,
                                                     uint8_t function,
                                                     uint16_t reg,
                                                     uint16_t value,
                                                     xm9505_rain_data_t *out)
{
    modbus_rtu_status_t status;

    if (function != MODBUS_FC_WRITE_SINGLE_REG) {
        return MODBUS_RTU_ERR_FUNC;
    }

    status = modbus_rtu_parse_write_single_response(frame,
                                                    len,
                                                    slave_addr,
                                                    function,
                                                    reg,
                                                    value);
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

modbus_rtu_status_t tf_modbus_xm9505_parse(const uint8_t *frame,
                                           uint32_t len, uint8_t slave_addr,
                                           xm9505_rain_data_t *out)
{
    return tf_modbus_parse_ranges(
        frame,
        len,
        slave_addr,
        XM9505_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm9505_rain_data_t, status),
        xm9505_register_maps,
        (uint16_t)(sizeof(xm9505_register_maps) / sizeof(xm9505_register_maps[0])));
}
