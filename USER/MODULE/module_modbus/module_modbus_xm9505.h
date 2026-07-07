#ifndef MODULE_MODBUS_XM9505_H
#define MODULE_MODBUS_XM9505_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM9505_REG_RAIN_TOTAL               0x0000U
#define XM9505_REG_RAIN_INSTANT             0x0001U
#define XM9505_REG_DIFF                     0x0002U
#define XM9505_REG_BASELINE                 0x0003U
#define XM9505_REG_RAINDROP_DYNAMIC         0x0004U
#define XM9505_REG_WATER_STATE              0x0005U
#define XM9505_REG_INTERVAL                 0x006AU
#define XM9505_REG_SENSITIVITY              0x006BU
#define XM9505_REG_WATER_THRESHOLD          0x006CU
#define XM9505_REG_HOUR_VALUE               0x006DU
#define XM9505_REG_12H_VALUE                0x006EU
#define XM9505_REG_ACCUMULATED              0x006FU
#define XM9505_RAIN_INSTANT_MAX_TENTH_MM    145U
#define XM9505_DIFF_MAX                     100U
#define XM9505_BASELINE_MAX                 10000U
#define XM9505_RAINDROP_DYNAMIC_MAX         1000U
#define XM9505_WATER_STATE_MAX              1000U
#define XM9505_MEASUREMENT_REG_COUNT        6U

typedef union {
    uint32_t raw;
    struct {
        unsigned rain_count       : 1;
        unsigned rain_instant     : 1;
        unsigned diff             : 1;
        unsigned baseline         : 1;
        unsigned raindrop_dynamic : 1;
        unsigned water_state      : 1;
        unsigned reserved         : 26;
    } bit;
} xm9505_rain_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    module_modbus_device_status_t status;
    xm9505_rain_marks_t marks;
    xm9505_rain_marks_t faults;
    uint16_t rain_count;       /* raw switch count, 1 count = 0.1 mm */
    uint16_t instant_tenth_mm;
    uint16_t diff;
    uint16_t baseline;
    uint16_t raindrop_dynamic;
    uint16_t water_state;
} xm9505_rain_data_t;

modbus_rtu_status_t module_modbus_parse_xm9505(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm9505_rain_data_t *out);
bool module_modbus_get_xm9505_data(xm9505_rain_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM9505_H */
