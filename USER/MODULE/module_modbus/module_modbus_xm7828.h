#ifndef MODULE_MODBUS_XM7828_H
#define MODULE_MODBUS_XM7828_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM7828_TEMP_MIN_CENTI_C             (-3000)
#define XM7828_TEMP_MAX_CENTI_C             8500
#define XM7828_HUMIDITY_MAX_CENTI_RH        10000U
#define XM7828_MEASUREMENT_REG_COUNT        2U

typedef union {
    uint32_t raw;
    struct {
        unsigned temperature : 1;
        unsigned humidity    : 1;
        unsigned reserved    : 30;
    } bit;
} xm7828_th_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    module_modbus_device_status_t status;
    xm7828_th_marks_t marks;
    xm7828_th_marks_t faults;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_rh;
} xm7828_th_data_t;

modbus_rtu_status_t module_modbus_parse_xm7828(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm7828_th_data_t *out);
bool module_modbus_get_xm7828_data(xm7828_th_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM7828_H */
