#ifndef TF_485_AC_H
#define TF_485_AC_H

#include "tf_485_device_common.h"

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

/**
 * @brief 空调参数有效位标记（bitfield union）
 *        标记 ac_params_t 中哪些字段已被成功解析填充。
 *        用于判读数据是否有效（全部清零表示从未收到过有效数据）。
 *
 *  使用方式（tf_485_master.c）：
 *    - 解析成功时置位对应 bit
 *    - 模块运行态 reset 时对 marks.raw 整体清零
 *    - 外部读取时检查 marks.raw != 0 判断数据有效
 */
typedef union {
    uint32_t raw; /**< 32 位原始值，可整体读写（清零/比较） */
    struct {
        unsigned cabinet_temp              : 1; /**< 机柜温度字段有效              */
        unsigned evaporator_temp           : 1; /**< 蒸发器温度字段有效            */
        unsigned condenser_temp            : 1; /**< 冷凝器温度字段有效            */
        unsigned humidity                  : 1; /**< 湿度字段有效                  */
        unsigned cooling_start             : 1; /**< 制冷启动温度阈值有效          */
        unsigned cooling_stop              : 1; /**< 制冷停止温度阈值有效          */
        unsigned heating_start             : 1; /**< 制热启动温度阈值有效          */
        unsigned heating_stop              : 1; /**< 制热停止温度阈值有效          */
        unsigned high_temp_alarm           : 1; /**< 高温告警阈值有效              */
        unsigned low_temp_alarm            : 1; /**< 低温告警阈值有效              */
        unsigned condenser_high_protect    : 1; /**< 冷凝器高压保护阈值有效        */
        unsigned evaporator_freeze_protect : 1; /**< 蒸发器防冻保护阈值有效        */
        unsigned dehumidify_start          : 1; /**< 除湿启动湿度阈值有效          */
        unsigned dehumidify_stop           : 1; /**< 除湿停止湿度阈值有效          */
        unsigned humidity_alarm            : 1; /**< 湿度告警阈值有效              */
        unsigned humidity_calibration      : 1; /**< 湿度校准值有效                */
        unsigned mode_control              : 1; /**< 模式控制寄存器有效            */
        unsigned reserved0                 : 1; /**< 保留                          */
        unsigned device_addr               : 1; /**< 设备地址字段有效              */
        unsigned reserved                  : 13;/**< 保留位                        */
    } bit;
} ac_param_marks_t;

/**
 * @brief 空调参数数据
 *        从空调设备读取的各项配置参数/阈值，包括温度、湿度、
 *        保护阈值和控制模式。
 *
 * @note  温度单位：十分之一摄氏度（deci-celsius）
 *        例：cabinet_temp_deci_c = 267 表示 26.7°C
 *        湿度单位：十分之一相对湿度（deci-RH）
 *        例：humidity_deci_rh = 532 表示 53.2%RH
 *
 *  marks 位标记指示各成员是否已成功解析（与 0 比较判空）。
 */
typedef struct {
    ac_param_marks_t marks;                    /**< 各字段有效位标记（非零 = 数据有效）*/

    /* ── 实测值 ── */
    int16_t cabinet_temp_deci_c;               /**< 机柜温度（十分之一 °C）       */
    int16_t evaporator_temp_deci_c;            /**< 蒸发器温度（十分之一 °C）     */
    int16_t condenser_temp_deci_c;             /**< 冷凝器温度（十分之一 °C）     */
    int16_t humidity_deci_rh;                  /**< 湿度（十分之一 %RH）          */

    /* ── 温度控制阈值 ── */
    int16_t cooling_start_deci_c;              /**< 制冷启动温度阈值（十分之一 °C）*/
    int16_t cooling_stop_deci_c;               /**< 制冷停止温度阈值              */
    int16_t heating_start_deci_c;              /**< 制热启动温度阈值              */
    int16_t heating_stop_deci_c;               /**< 制热停止温度阈值              */

    /* ── 告警阈值 ── */
    int16_t high_temp_alarm_deci_c;            /**< 高温告警阈值                  */
    int16_t low_temp_alarm_deci_c;             /**< 低温告警阈值                  */
    int16_t condenser_high_protect_deci_c;     /**< 冷凝器高压保护阈值            */
    int16_t evaporator_freeze_protect_deci_c;  /**< 蒸发器防冻保护阈值            */

    /* ── 湿度控制阈值 ── */
    int16_t dehumidify_start_deci_rh;          /**< 除湿启动湿度阈值              */
    int16_t dehumidify_stop_deci_rh;           /**< 除湿停止湿度阈值              */
    int16_t humidity_alarm_deci_rh;            /**< 湿度告警阈值                  */
    int16_t humidity_calibration;              /**< 湿度校准值                    */

    /* ── 控制参数 ── */
    uint16_t mode_control;                     /**< 模式控制寄存器值              */
    uint16_t device_addr;                      /**< 空调 Modbus 设备地址          */
} ac_params_t;

/**
 * @brief 空调状态有效位标记（bitfield union）
 *        标记 ac_status_t 中哪些状态位已被成功解析。
 *        与 ac_param_marks_t 用法相同。
 */
typedef union {
    uint32_t raw; /**< 32 位原始值，可整体读写 */
    struct {
        unsigned cooling_on                    : 1; /**< 制冷运行中                    */
        unsigned heater_on                     : 1; /**< 加热运行中                    */
        unsigned inner_fan_on                  : 1; /**< 内风机运行中                  */
        unsigned outer_fan_on                  : 1; /**< 外风机运行中                  */
        unsigned monitor_shutdown              : 1; /**< 监控关机状态                  */
        unsigned cabinet_sensor_fault          : 1; /**< 机柜温度传感器故障            */
        unsigned evaporator_sensor_fault       : 1; /**< 蒸发器温度传感器故障          */
        unsigned condenser_sensor_fault        : 1; /**< 冷凝器温度传感器故障          */
        unsigned humidity_sensor_fault         : 1; /**< 湿度传感器故障                */
        unsigned humidity_high_alarm           : 1; /**< 湿度高告警                    */
        unsigned condenser_high_pressure_alarm : 1; /**< 冷凝器高压告警                */
        unsigned evaporator_freeze_alarm       : 1; /**< 蒸发器冻结告警                */
        unsigned cooling_alarm                 : 1; /**< 制冷故障告警                  */
        unsigned high_temp_alarm               : 1; /**< 高温告警                      */
        unsigned low_temp_alarm                : 1; /**< 低温告警                      */
        unsigned reserved                      : 17;/**< 保留位                        */
    } bit;
} ac_status_marks_t;

/**
 * @brief 空调运行状态数据
 *        从空调设备读取的各项运行状态标志，
 *        包括各部件启停状态和故障告警。
 *
 *  marks 位标记指示各成员是否已成功解析。
 *  外部通过 tf_485_ac_get_status() 读取并检查 marks.raw != 0。
 */
typedef struct {
    ac_status_marks_t marks;                   /**< 各字段有效位标记              */

    /* ── 运行状态 ── */
    bool cooling_on;                           /**< 制冷运行中                    */
    bool heater_on;                            /**< 加热运行中                    */
    bool inner_fan_on;                         /**< 内风机运行中                  */
    bool outer_fan_on;                         /**< 外风机运行中                  */
    bool monitor_shutdown;                     /**< 监控关机状态（电源监控关机）  */

    /* ── 传感器故障 ── */
    bool cabinet_sensor_fault;                 /**< 机柜温度传感器故障            */
    bool evaporator_sensor_fault;              /**< 蒸发器温度传感器故障          */
    bool condenser_sensor_fault;               /**< 冷凝器温度传感器故障          */
    bool humidity_sensor_fault;                /**< 湿度传感器故障                */

    /* ── 告警状态 ── */
    bool humidity_high_alarm;                  /**< 湿度高告警                    */
    bool condenser_high_pressure_alarm;        /**< 冷凝器高压告警                */
    bool evaporator_freeze_alarm;              /**< 蒸发器冻结告警                */
    bool cooling_alarm;                        /**< 制冷故障告警                  */
    bool high_temp_alarm;                      /**< 高温告警                      */
    bool low_temp_alarm;                       /**< 低温告警                      */
} ac_status_t;

/**
 * @brief 空调设备缓存
 *        AC 作为一个 Modbus 设备持有设备级地址与运行状态；
 *        参数数据和运行状态数据是该设备的两个内部数据分组。
 */
typedef struct {
    uint8_t slave_addr;                              /**< 从机 Modbus 地址              */
    tf_485_dev_status_t status;      /**< 设备运行状态（在线、传感器故障）*/
    ac_params_t params;                        /**< 空调参数数据                  */
    ac_status_t run_status;                    /**< 空调运行状态数据              */
} ac_device_t;

bool tf_485_ac_read_params(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_485_ac_read_status(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_485_ac_write_status(uint8_t slave_addr, uint16_t status_ref,
                            bool on);
bool tf_485_ac_write_param(uint8_t slave_addr, uint16_t param_ref,
                           int16_t value);
modbus_rtu_status_t tf_485_ac_parse_params(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           uint16_t start_ref,
                                           uint16_t requested_count,
                                           ac_params_t *out);
modbus_rtu_status_t tf_485_ac_parse_status(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           uint16_t start_ref,
                                           ac_status_t *out);
modbus_rtu_status_t tf_485_ac_parse_write_ack(const uint8_t *frame,
                                              uint32_t len,
                                              uint8_t slave_addr,
                                              uint8_t function,
                                              uint16_t ref,
                                              uint16_t value,
                                              ac_device_t *out);
bool tf_485_ac_get_params(ac_params_t *out);
bool tf_485_ac_get_status(ac_status_t *out);
bool tf_485_ac_get(ac_device_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_485_AC_H */
