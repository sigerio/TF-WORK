#ifndef TF_MODBUS_AC_H
#define TF_MODBUS_AC_H

#include "tf_modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AC_MAX_READ_REG_COUNT               50U

/* AC 地址采用协议表的参考编号（reference-style），
   仅在构建请求帧时转换为 Modbus PDU 偏移。 */
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
#define AC_PARAM_SEG_CONTROL_START          AC_PARAM_REG_COOLING_START
#define AC_PARAM_SEG_CONTROL_COUNT          \
    ((uint16_t)(AC_PARAM_REG_MODE_CONTROL - AC_PARAM_REG_COOLING_START + 1U))
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

/** @brief 空调参数有效位标记（bitfield union），非零 = 数据有效 */
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

/**
 * @brief 空调参数数据
 *        温度单位：十分之一摄氏度；湿度单位：十分之一相对湿度。
 */
typedef struct {
    ac_param_marks_t marks;

    /* ── 实测值 ── */
    int16_t cabinet_temp_deci_c;
    int16_t evaporator_temp_deci_c;
    int16_t condenser_temp_deci_c;
    int16_t humidity_deci_rh;

    /* ── 温度控制阈值 ── */
    int16_t cooling_start_deci_c;
    int16_t cooling_stop_deci_c;
    int16_t heating_start_deci_c;
    int16_t heating_stop_deci_c;

    /* ── 告警阈值 ── */
    int16_t high_temp_alarm_deci_c;
    int16_t low_temp_alarm_deci_c;
    int16_t condenser_high_protect_deci_c;
    int16_t evaporator_freeze_protect_deci_c;

    /* ── 湿度控制阈值 ── */
    int16_t dehumidify_start_deci_rh;
    int16_t dehumidify_stop_deci_rh;
    int16_t humidity_alarm_deci_rh;
    int16_t humidity_calibration;

    /* ── 控制参数 ── */
    uint16_t mode_control;
    uint16_t device_addr;
} ac_params_t;

/** @brief 空调状态有效位标记（bitfield union） */
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
        unsigned reserved                      : 17;
    } bit;
} ac_status_marks_t;

/** @brief 空调运行状态数据（各部件启停状态和故障告警） */
typedef struct {
    ac_status_marks_t marks;

    /* ── 运行状态 ── */
    bool cooling_on;
    bool heater_on;
    bool inner_fan_on;
    bool outer_fan_on;
    bool monitor_shutdown;

    /* ── 传感器故障 ── */
    bool cabinet_sensor_fault;
    bool evaporator_sensor_fault;
    bool condenser_sensor_fault;
    bool humidity_sensor_fault;

    /* ── 告警状态 ── */
    bool humidity_high_alarm;
    bool condenser_high_pressure_alarm;
    bool evaporator_freeze_alarm;
    bool cooling_alarm;
    bool high_temp_alarm;
    bool low_temp_alarm;
} ac_status_t;

/** @brief 空调设备缓存（设备级地址与运行状态 + 参数/状态两个数据分组） */
typedef struct {
    uint8_t slave_addr;
    tf_modbus_dev_status_t status;
    ac_params_t params;
    ac_status_t run_status;
} ac_device_t;

bool tf_modbus_ac_read_params(uint8_t slave_addr, uint16_t start_reg,
                              uint16_t reg_count);
bool tf_modbus_ac_read_status(uint8_t slave_addr, uint16_t start_reg,
                              uint16_t reg_count);
bool tf_modbus_ac_write_status(uint8_t slave_addr, uint16_t status_ref,
                               bool on);
bool tf_modbus_ac_write_param(uint8_t slave_addr, uint16_t param_ref,
                              int16_t value);
modbus_rtu_status_t tf_modbus_ac_parse_params(const uint8_t *frame,
                                              uint32_t len,
                                              uint8_t slave_addr,
                                              uint16_t start_ref,
                                              uint16_t requested_count,
                                              ac_params_t *out);
modbus_rtu_status_t tf_modbus_ac_parse_status(const uint8_t *frame,
                                              uint32_t len,
                                              uint8_t slave_addr,
                                              uint16_t start_ref,
                                              ac_status_t *out);
modbus_rtu_status_t tf_modbus_ac_parse_write_ack(const uint8_t *frame,
                                                 uint32_t len,
                                                 uint8_t slave_addr,
                                                 uint8_t function,
                                                 uint16_t ref,
                                                 uint16_t value,
                                                 ac_device_t *out);
bool tf_modbus_ac_get_params(ac_params_t *out);
bool tf_modbus_ac_get_status(ac_status_t *out);
bool tf_modbus_ac_get(ac_device_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_MODBUS_AC_H */
