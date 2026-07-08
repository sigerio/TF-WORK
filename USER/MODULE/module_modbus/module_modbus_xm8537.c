#include "module_modbus_xm8537.h"

#include <stddef.h>
#include <string.h>

#include "modbus_tx_queue.h"

#define XM8537_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm8537_water_data_t, field_), \
      (min_), (max_), MODULE_MODBUS_REG_VALUE_U16 }

static const module_modbus_reg_range_item_t s_xm8537_ranges[] = {
    XM8537_RANGE_ITEM(0U, raw,
                      XM8537_WATER_STATE_NORMAL,
                      XM8537_WATER_STATE_DETECTED),
};

bool module_modbus_xm8537_read(uint8_t addr, uint16_t start_reg,
                                            uint16_t reg_count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    modbus_response_expect_t waiting;
    uint32_t len;

    if (addr == 0U) {
        return false;
    }

    len = modbus_rtu_build_read_request(addr,
                                        MODBUS_FC_READ_HOLDING_REGS,
                                        start_reg,
                                        reg_count,
                                        frame,
                                        sizeof(frame));
    if (len == 0U) {
        return false;
    }

    memset(&waiting, 0, sizeof(waiting));
    waiting.addr = addr;
    waiting.device = MODULE_MODBUS_DEV_XM8537_WATER;
    waiting.function = MODULE_MODBUS_POLL_READ_REGS;
    waiting.start = start_reg;
    waiting.count = reg_count;
    waiting.expected_func = MODBUS_FC_READ_HOLDING_REGS;
    waiting.expected_bytes = (uint8_t)(reg_count * 2U);

    return modbus_tx_queue_enqueue(frame, (uint16_t)len, &waiting);
}

modbus_rtu_status_t module_modbus_parse_xm8537(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm8537_water_data_t *out)
{
    modbus_rtu_status_t status;

    status = module_modbus_parse_regs_with_ranges(
        frame,
        len,
        addr,
        XM8537_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm8537_water_data_t, status),
        s_xm8537_ranges,
        (uint16_t)(sizeof(s_xm8537_ranges) / sizeof(s_xm8537_ranges[0])));
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    if (!out->status.sensor_fault) {
        out->water_detected = (out->raw == XM8537_WATER_STATE_DETECTED);
    }

    return MODBUS_RTU_OK;
}
