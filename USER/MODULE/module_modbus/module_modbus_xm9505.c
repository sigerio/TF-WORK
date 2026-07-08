#include "module_modbus_xm9505.h"

#include <stddef.h>
#include <string.h>

#include "modbus_tx_queue.h"

#define XM9505_RANGE_ITEM(index_, field_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm9505_rain_data_t, field_), \
      (min_), (max_), MODULE_MODBUS_REG_VALUE_U16 }

static const module_modbus_reg_range_item_t s_xm9505_ranges[] = {
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

bool module_modbus_xm9505_read(uint8_t addr, uint16_t start_reg,
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
    waiting.device = MODULE_MODBUS_DEV_XM9505_RAIN;
    waiting.function = MODULE_MODBUS_POLL_READ_REGS;
    waiting.start = start_reg;
    waiting.count = reg_count;
    waiting.expected_func = MODBUS_FC_READ_HOLDING_REGS;
    waiting.expected_bytes = (uint8_t)(reg_count * 2U);

    return modbus_tx_queue_enqueue(frame, (uint16_t)len, &waiting);
}

modbus_rtu_status_t module_modbus_parse_xm9505(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm9505_rain_data_t *out)
{
    return module_modbus_parse_regs_with_ranges(
        frame,
        len,
        addr,
        XM9505_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm9505_rain_data_t, status),
        s_xm9505_ranges,
        (uint16_t)(sizeof(s_xm9505_ranges) / sizeof(s_xm9505_ranges[0])));
}
