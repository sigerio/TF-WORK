#include "module_modbus_xm8189.h"

#include <stddef.h>

#define XM8189_RANGE_ITEM(index_, field_, mask_, min_, max_) \
    { (index_), (uint16_t)offsetof(xm8189_wind_data_t, field_), \
      (mask_), (mask_), (min_), (max_), MODULE_MODBUS_REG_VALUE_U16 }

static const module_modbus_reg_range_item_t s_xm8189_ranges[] = {
    XM8189_RANGE_ITEM(0U, speed_centi, (1UL << 0),
                      0,
                      XM8189_SPEED_MAX_CENTI_MS),
    XM8189_RANGE_ITEM(1U, direction_centi_deg, (1UL << 1),
                      0,
                      XM8189_DIRECTION_MAX_CENTI_DEG),
};

modbus_rtu_status_t module_modbus_parse_xm8189(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm8189_wind_data_t *out)
{
    return module_modbus_parse_regs_with_ranges(
        frame,
        len,
        addr,
        XM8189_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm8189_wind_data_t, status),
        (uint16_t)offsetof(xm8189_wind_data_t, marks),
        (uint16_t)offsetof(xm8189_wind_data_t, faults),
        s_xm8189_ranges,
        (uint16_t)(sizeof(s_xm8189_ranges) / sizeof(s_xm8189_ranges[0])));
}
