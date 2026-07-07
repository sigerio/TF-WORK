#ifndef MODULE_MODBUS_XM8189_H
#define MODULE_MODBUS_XM8189_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8189_SPEED_MAX_CENTI_MS           3000U
#define XM8189_DIRECTION_MAX_CENTI_DEG      36000U
#define XM8189_MEASUREMENT_REG_COUNT        2U

typedef union {
    uint32_t raw;
    struct {
        unsigned speed     : 1;
        unsigned direction : 1;
        unsigned reserved  : 30;
    } bit;
} xm8189_wind_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    module_modbus_device_status_t status;
    xm8189_wind_marks_t marks;
    xm8189_wind_marks_t faults;
    uint16_t speed_centi;
    uint16_t direction_centi_deg;
} xm8189_wind_data_t;

modbus_rtu_status_t module_modbus_parse_xm8189(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm8189_wind_data_t *out);
bool module_modbus_get_xm8189_data(xm8189_wind_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM8189_H */
