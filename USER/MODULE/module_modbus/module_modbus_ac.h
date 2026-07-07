#ifndef MODULE_MODBUS_AC_H
#define MODULE_MODBUS_AC_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MAX_READ_REG_COUNT               50U

/* AC addresses below use the reference-style numbers from the protocol table.
   They are converted to Modbus PDU offsets only when building request frames. */
#define AC_PARAM_READ_BASE_REF              30001U
#define AC_STATUS_READ_BASE_REF             10001U

#define AC_PARAM_REG_CABINET_TEMP           30001U
#define AC_PARAM_REG_EVAPORATOR_TEMP        30002U
#define AC_PARAM_REG_CONDENSER_TEMP         30003U
#define AC_PARAM_REG_HUMIDITY               30009U
#define AC_PARAM_REG_COOLING_START          30014U
#define AC_PARAM_REG_COOLING_STOP           30015U
#define AC_PARAM_REG_HEATING_START          30016U
#define AC_PARAM_REG_HEATING_STOP           30017U
#define AC_PARAM_REG_HIGH_TEMP_ALARM        30018U
#define AC_PARAM_REG_LOW_TEMP_ALARM         30019U
#define AC_PARAM_REG_CONDENSER_HIGH_PROTECT 30020U
#define AC_PARAM_REG_EVAPORATOR_FREEZE      30021U
#define AC_PARAM_REG_DEHUMIDIFY_START       30022U
#define AC_PARAM_REG_DEHUMIDIFY_STOP        30023U
#define AC_PARAM_REG_HUMIDITY_ALARM         30024U
#define AC_PARAM_REG_HUMIDITY_CALIBRATION   30025U
#define AC_PARAM_REG_MODE_CONTROL           30026U
#define AC_PARAM_REG_RESERVED_30027         30027U
#define AC_PARAM_REG_DEVICE_ADDR            30028U

#define AC_PARAM_SEG_MEASURE_START          AC_PARAM_REG_CABINET_TEMP
#define AC_PARAM_SEG_MEASURE_COUNT          3U
#define AC_PARAM_SEG_HUMIDITY_START         AC_PARAM_REG_HUMIDITY
#define AC_PARAM_SEG_HUMIDITY_COUNT         1U
#define AC_STATUS_COOLING_ON                10035U
#define AC_STATUS_HEATER_ON                 10036U
#define AC_STATUS_INNER_FAN_ON              10037U
#define AC_STATUS_OUTER_FAN_ON              10038U
#define AC_STATUS_POWER_MONITOR             10042U
#define AC_STATUS_CABINET_SENSOR_FAULT      10043U
#define AC_STATUS_EVAPORATOR_SENSOR_FAULT   10044U
#define AC_STATUS_CONDENSER_SENSOR_FAULT    10045U
#define AC_STATUS_HUMIDITY_SENSOR_FAULT     10047U
#define AC_STATUS_HUMIDITY_HIGH_ALARM       10048U
#define AC_STATUS_CONDENSER_PRESSURE_ALARM  10051U
#define AC_STATUS_EVAPORATOR_FREEZE_ALARM   10052U
#define AC_STATUS_COOLING_ALARM             10053U
#define AC_STATUS_HIGH_TEMP_ALARM           10054U
#define AC_STATUS_LOW_TEMP_ALARM            10055U
#define AC_STATUS_RESERVED_END              10058U

#define AC_STATUS_SEG_ALL_START             AC_STATUS_COOLING_ON
#define AC_STATUS_SEG_ALL_COUNT             \
    ((uint16_t)(AC_STATUS_RESERVED_END - AC_STATUS_COOLING_ON + 1U))

typedef union {
    uint32_t raw;
    struct {
        unsigned cabinet_temp              : 1;
        unsigned evaporator_temp           : 1;
        unsigned condenser_temp            : 1;
        unsigned humidity                  : 1;
        unsigned cooling_start             : 1;
        unsigned cooling_stop              : 1;
        unsigned heating_start             : 1;
        unsigned heating_stop              : 1;
        unsigned high_temp_alarm           : 1;
        unsigned low_temp_alarm            : 1;
        unsigned condenser_high_protect    : 1;
        unsigned evaporator_freeze_protect : 1;
        unsigned dehumidify_start          : 1;
        unsigned dehumidify_stop           : 1;
        unsigned humidity_alarm            : 1;
        unsigned humidity_calibration      : 1;
        unsigned mode_control              : 1;
        unsigned reserved0                 : 1;
        unsigned device_addr               : 1;
        unsigned reserved                  : 13;
    } bit;
} ac_param_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    ac_param_marks_t marks;
    int16_t cabinet_temp_deci_c;
    int16_t evaporator_temp_deci_c;
    int16_t condenser_temp_deci_c;
    int16_t humidity_deci_rh;
    int16_t cooling_start_deci_c;
    int16_t cooling_stop_deci_c;
    int16_t heating_start_deci_c;
    int16_t heating_stop_deci_c;
    int16_t high_temp_alarm_deci_c;
    int16_t low_temp_alarm_deci_c;
    int16_t condenser_high_protect_deci_c;
    int16_t evaporator_freeze_protect_deci_c;
    int16_t dehumidify_start_deci_rh;
    int16_t dehumidify_stop_deci_rh;
    int16_t humidity_alarm_deci_rh;
    int16_t humidity_calibration;
    uint16_t mode_control;
    uint16_t device_addr;
} ac_params_t;

typedef union {
    uint32_t raw;
    struct {
        unsigned cooling_on                    : 1;
        unsigned heater_on                     : 1;
        unsigned inner_fan_on                  : 1;
        unsigned outer_fan_on                  : 1;
        unsigned monitor_shutdown              : 1;
        unsigned cabinet_sensor_fault          : 1;
        unsigned evaporator_sensor_fault       : 1;
        unsigned condenser_sensor_fault        : 1;
        unsigned humidity_sensor_fault         : 1;
        unsigned humidity_high_alarm           : 1;
        unsigned condenser_high_pressure_alarm : 1;
        unsigned evaporator_freeze_alarm       : 1;
        unsigned cooling_alarm                 : 1;
        unsigned high_temp_alarm               : 1;
        unsigned low_temp_alarm                : 1;
        unsigned comm_fault                    : 1;
        unsigned reserved                      : 16;
    } bit;
} ac_status_marks_t;

typedef struct {
    module_modbus_device_config_t config;
    ac_status_marks_t marks;
    bool cooling_on;
    bool heater_on;
    bool inner_fan_on;
    bool outer_fan_on;
    bool monitor_shutdown;
    bool cabinet_sensor_fault;
    bool evaporator_sensor_fault;
    bool condenser_sensor_fault;
    bool humidity_sensor_fault;
    bool humidity_high_alarm;
    bool condenser_high_pressure_alarm;
    bool evaporator_freeze_alarm;
    bool cooling_alarm;
    bool high_temp_alarm;
    bool low_temp_alarm;
    bool comm_fault;
} ac_status_t;

bool module_modbus_ac_read_params(uint8_t addr, uint16_t start_ref,
                                  uint16_t count);
bool module_modbus_ac_read_status(uint8_t addr, uint16_t start_ref,
                                  uint16_t count);
modbus_rtu_status_t module_modbus_ac_parse_params(const uint8_t *frame,
                                                  uint32_t len, uint8_t addr,
                                                  uint16_t start_ref,
                                                  uint16_t requested_count,
                                                  ac_params_t *out);
modbus_rtu_status_t module_modbus_ac_parse_status(const uint8_t *frame,
                                                  uint32_t len, uint8_t addr,
                                                  uint16_t start_ref,
                                                  ac_status_t *out);
bool module_modbus_get_ac_params(ac_params_t *out);
bool module_modbus_get_ac_status(ac_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_AC_H */
