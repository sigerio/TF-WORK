#include "module_modbus_xm8537.h"

#include <stddef.h>

#define XM8537_RANGE_ITEM(index_, field_, mask_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm8537_water_data_t, field_), \
      (mask_), (mask_), (min_), (max_), MODULE_MODBUS_REG_VALUE_U16 }

static const module_modbus_reg_range_item_t s_xm8537_ranges[] = {
    XM8537_RANGE_ITEM(0U, raw, (1UL << 0),
                      XM8537_WATER_STATE_NORMAL,
                      XM8537_WATER_STATE_DETECTED),
};

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
        (uint16_t)offsetof(xm8537_water_data_t, marks),
        (uint16_t)offsetof(xm8537_water_data_t, faults),
        s_xm8537_ranges,
        (uint16_t)(sizeof(s_xm8537_ranges) / sizeof(s_xm8537_ranges[0])));
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    if (out->marks.bit.state != 0U) {
        out->water_detected = (out->raw == XM8537_WATER_STATE_DETECTED);
    }

    return MODBUS_RTU_OK;
}
