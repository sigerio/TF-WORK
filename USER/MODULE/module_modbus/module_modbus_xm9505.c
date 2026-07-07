#include "module_modbus_xm9505.h"

#include <stddef.h>

#define XM9505_RANGE_ITEM(index_, field_, mask_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm9505_rain_data_t, field_), \
      (mask_), (mask_), (min_), (max_), MODULE_MODBUS_REG_VALUE_U16 }

static const module_modbus_reg_range_item_t s_xm9505_ranges[] = {
    XM9505_RANGE_ITEM(0U, rain_count, (1UL << 0), 0, 0xFFFF),
    XM9505_RANGE_ITEM(1U, instant_tenth_mm, (1UL << 1),
                      0,
                      XM9505_RAIN_INSTANT_MAX_TENTH_MM),
    XM9505_RANGE_ITEM(2U, diff, (1UL << 2), 0, XM9505_DIFF_MAX),
    XM9505_RANGE_ITEM(3U, baseline, (1UL << 3), 0, XM9505_BASELINE_MAX),
    XM9505_RANGE_ITEM(4U, raindrop_dynamic, (1UL << 4),
                      0,
                      XM9505_RAINDROP_DYNAMIC_MAX),
    XM9505_RANGE_ITEM(5U, water_state, (1UL << 5), 0, XM9505_WATER_STATE_MAX),
};

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
        (uint16_t)offsetof(xm9505_rain_data_t, marks),
        (uint16_t)offsetof(xm9505_rain_data_t, faults),
        s_xm9505_ranges,
        (uint16_t)(sizeof(s_xm9505_ranges) / sizeof(s_xm9505_ranges[0])));
}
