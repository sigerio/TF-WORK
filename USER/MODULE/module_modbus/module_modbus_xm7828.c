#include "module_modbus_xm7828.h"

#include <stddef.h>

#define XM7828_RANGE_ITEM(index_, field_, mask_, min_, max_, type_) \
    { (index_), (uint16_t)offsetof(xm7828_th_data_t, field_), \
      (mask_), (mask_), (min_), (max_), (type_) }

static const module_modbus_reg_range_item_t s_xm7828_ranges[] = {
    XM7828_RANGE_ITEM(0U, temperature_centi_c, (1UL << 0),
                      XM7828_TEMP_MIN_CENTI_C,
                      XM7828_TEMP_MAX_CENTI_C,
                      MODULE_MODBUS_REG_VALUE_I16),
    XM7828_RANGE_ITEM(1U, humidity_centi_rh, (1UL << 1),
                      0,
                      XM7828_HUMIDITY_MAX_CENTI_RH,
                      MODULE_MODBUS_REG_VALUE_U16),
};

modbus_rtu_status_t module_modbus_parse_xm7828(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm7828_th_data_t *out)
{
    return module_modbus_parse_regs_with_ranges(
        frame,
        len,
        addr,
        XM7828_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm7828_th_data_t, status),
        (uint16_t)offsetof(xm7828_th_data_t, marks),
        (uint16_t)offsetof(xm7828_th_data_t, faults),
        s_xm7828_ranges,
        (uint16_t)(sizeof(s_xm7828_ranges) / sizeof(s_xm7828_ranges[0])));
}
