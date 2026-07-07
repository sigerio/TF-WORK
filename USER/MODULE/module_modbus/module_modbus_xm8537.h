#ifndef MODULE_MODBUS_XM8537_H
#define MODULE_MODBUS_XM8537_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8537_WATER_STATE_NORMAL           0x0000U
#define XM8537_WATER_STATE_DETECTED         0x0001U
#define XM8537_MEASUREMENT_REG_COUNT        1U

typedef union {
    uint32_t raw;
    struct {
        unsigned state    : 1;
        unsigned reserved : 31;
    } bit;
} xm8537_water_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    module_modbus_device_status_t status;
    xm8537_water_marks_t marks;
    xm8537_water_marks_t faults;
    uint16_t raw;
    bool water_detected;
} xm8537_water_data_t;

modbus_rtu_status_t module_modbus_parse_xm8537(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm8537_water_data_t *out);
bool module_modbus_get_xm8537_data(xm8537_water_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM8537_H */
