#ifndef MODBUS_TIS_H
#define MODBUS_TIS_H

#include "COMM/modbus/devices/modbus_common.h"
#include "COMM/modbus/modbus_config.h"

#define TIS_WRITE_FRAME_MAX (9U + 2U * MODULE_MODBUS_TIS_MAX_REGS)

/* 地址、类型和系数以 TIS V1.00 表格为准；不使用附件示例。 */
enum {
    TIS_REG_RATED_PV_VOLTAGE = 0x3002U,
    TIS_REG_RATED_PV_CURRENT = 0x3003U,
    TIS_REG_RATED_PV_POWER = 0x3004U,
    TIS_REG_RATED_BATTERY_VOLTAGE = 0x3006U,
    TIS_REG_RATED_CHARGE_CURRENT = 0x3007U,
    TIS_REG_RATED_CHARGE_POWER = 0x3008U,
    TIS_REG_RATED_LOAD_VOLTAGE = 0x300AU,
    TIS_REG_RATED_LOAD_CURRENT = 0x300BU,
    TIS_REG_RATED_LOAD_POWER = 0x300CU,
    TIS_REG_PV1_INPUT_VOLTAGE = 0x3100U,
    TIS_REG_PV1_INPUT_CURRENT = 0x3101U,
    TIS_REG_PV1_INPUT_POWER = 0x3102U,
    TIS_REG_PV1_OUTPUT_VOLTAGE = 0x3104U,
    TIS_REG_PV1_OUTPUT_CURRENT = 0x3105U,
    TIS_REG_PV1_OUTPUT_POWER = 0x3106U,
    TIS_REG_PV2_INPUT_VOLTAGE = 0x3108U,
    TIS_REG_PV2_INPUT_CURRENT = 0x3109U,
    TIS_REG_PV2_INPUT_POWER = 0x310AU,
    TIS_REG_PV2_OUTPUT_VOLTAGE = 0x310CU,
    TIS_REG_PV2_OUTPUT_CURRENT = 0x310DU,
    TIS_REG_PV2_OUTPUT_POWER = 0x310EU,
    TIS_REG_LOAD_VOLTAGE = 0x3110U,
    TIS_REG_LOAD_CURRENT = 0x3111U,
    TIS_REG_LOAD_POWER = 0x3112U,
    TIS_REG_BATTERY_VOLTAGE = 0x3114U,
    TIS_REG_BATTERY_CHARGE_CURRENT = 0x3115U,
    TIS_REG_BATTERY_DISCHARGE_CURRENT = 0x3116U,
    TIS_REG_BATTERY_TOTAL_CURRENT = 0x3117U,
    TIS_REG_BATTERY_TEMPERATURE = 0x3118U,
    TIS_REG_BATTERY_SOC = 0x3119U,
    TIS_REG_DEVICE_TEMPERATURE = 0x311AU,
    TIS_REG_SYSTEM_VOLTAGE = 0x311DU,
    TIS_REG_MAX_PV_VOLTAGE = 0x311EU,
    TIS_REG_PV_CHARGE_CURRENT = 0x311FU,
    TIS_REG_PV_CHARGE_POWER = 0x3120U,
    TIS_REG_BATTERY_STATE = 0x3200U,
    TIS_REG_LOAD_STATE = 0x3201U,
    TIS_REG_CHARGE1_STATE = 0x3202U,
    TIS_REG_CHARGE2_STATE = 0x3203U,
    TIS_REG_DEVICE_STATE = 0x3205U,
    TIS_REG_TODAY_MAX_PV_VOLTAGE = 0x3300U,
    TIS_REG_TODAY_MAX_BATTERY_VOLTAGE = 0x3301U,
    TIS_REG_TODAY_MIN_BATTERY_VOLTAGE = 0x3302U,
    TIS_REG_TODAY_CONSUMPTION = 0x3303U,
    TIS_REG_MONTH_CONSUMPTION = 0x3305U,
    TIS_REG_YEAR_CONSUMPTION = 0x3307U,
    TIS_REG_TOTAL_CONSUMPTION = 0x3309U,
    TIS_REG_TODAY_GENERATION = 0x330BU,
    TIS_REG_MONTH_GENERATION = 0x330DU,
    TIS_REG_YEAR_GENERATION = 0x330FU,
    TIS_REG_TOTAL_GENERATION = 0x3311U,
    TIS_REG_LEARNED_CAPACITY = 0x3313U,
    TIS_REG_BMS_CELL_COUNT = 0x3400U,
    TIS_REG_BMS_VOLTAGE = 0x3401U,
    TIS_REG_BMS_CURRENT = 0x3402U,
    TIS_REG_BMS_POWER = 0x3403U,
    TIS_REG_BMS_FULL_CAPACITY = 0x3405U,
    TIS_REG_BMS_SOC = 0x3406U,
    TIS_REG_BMS_REMAINING_MINUTES = 0x3407U,
    TIS_REG_BMS_MAX_CELL_TEMPERATURE = 0x3408U,
    TIS_REG_BMS_MIN_CELL_TEMPERATURE = 0x3409U,
    TIS_REG_BMS_BALANCE_TEMPERATURE = 0x340AU,
    TIS_REG_BMS_AMBIENT_TEMPERATURE = 0x340BU,
    TIS_REG_BMS_MOS_TEMPERATURE = 0x340CU,
    TIS_REG_BMS_CYCLE_COUNT = 0x340DU,
    TIS_REG_BMS_BALANCE_STATE = 0x340EU,
    TIS_REG_BMS_VOLTAGE_STATE = 0x340FU,
    TIS_REG_BMS_CURRENT_STATE = 0x3410U,
    TIS_REG_BMS_MOS_STATE = 0x3411U,
    TIS_REG_BMS_CELL_TEMPERATURE_STATE = 0x3412U,
    TIS_REG_BMS_BALANCE_TEMPERATURE_STATE = 0x3413U,
    TIS_REG_BMS_AMBIENT_TEMPERATURE_STATE = 0x3414U,
    TIS_REG_BMS_MOS_TEMPERATURE_STATE = 0x3415U,
    TIS_REG_BMS_CELL_STATE = 0x3416U,
    TIS_REG_BMS_PROTOCOL = 0x3417U,
    TIS_REG_BMS_BATTERY_STATE = 0x3418U,
    TIS_REG_BMS_PACK_MODE = 0x3419U,
    TIS_REG_BMS_GROUP_VOLTAGE = 0x341AU,
    TIS_REG_BMS_GROUP_CURRENT = 0x341BU,
    TIS_REG_BMS_UNDERVOLTAGE_LIMIT = 0x341CU,
    TIS_REG_BMS_OVERVOLTAGE_ALARM = 0x341DU,
    TIS_REG_BMS_CHARGE_CURRENT_LIMIT = 0x341EU,
    TIS_REG_BMS_DISCHARGE_CURRENT_LIMIT = 0x341FU,
    TIS_REG_BMS_GROUP_UNDERVOLTAGE_LIMIT = 0x3420U,
    TIS_REG_BMS_GROUP_OVERVOLTAGE_LIMIT = 0x3421U,
    TIS_REG_BMS_GROUP_CHARGE_CURRENT_LIMIT = 0x3422U,
    TIS_REG_BMS_GROUP_DISCHARGE_CURRENT_LIMIT = 0x3423U,
    TIS_REG_BATTERY_TYPE = 0x9000U,
    TIS_REG_BATTERY_CAPACITY = 0x9001U,
    TIS_REG_TEMPERATURE_COMPENSATION = 0x9002U,
    TIS_REG_TEMPERATURE_UNIT = 0x9003U,
    TIS_REG_BACKLIGHT_SECONDS = 0x9004U,
    TIS_REG_DISPLAY_CYCLE_SECONDS = 0x9005U,
    TIS_REG_VOLTAGE_LEVEL = 0x9006U,
    TIS_REG_OVERVOLTAGE_DISCONNECT = 0x9007U,
    TIS_REG_CHARGE_VOLTAGE_LIMIT = 0x9008U,
    TIS_REG_OVERVOLTAGE_RECONNECT = 0x9009U,
    TIS_REG_EQUALIZE_VOLTAGE = 0x900AU,
    TIS_REG_BOOST_VOLTAGE = 0x900BU,
    TIS_REG_FLOAT_VOLTAGE = 0x900CU,
    TIS_REG_BOOST_RECONNECT_VOLTAGE = 0x900DU,
    TIS_REG_LOW_VOLTAGE_RECONNECT = 0x900EU,
    TIS_REG_UNDERVOLTAGE_ALARM_RECONNECT = 0x900FU,
    TIS_REG_UNDERVOLTAGE_ALARM = 0x9010U,
    TIS_REG_LOW_VOLTAGE_DISCONNECT = 0x9011U,
    TIS_REG_DISCHARGE_VOLTAGE_LIMIT = 0x9012U,
    TIS_REG_ALLOWED_CHARGE_CURRENT = 0x9013U,
    TIS_REG_EQUALIZE_MINUTES = 0x9014U,
    TIS_REG_BOOST_MINUTES = 0x9015U,
    TIS_REG_LITHIUM_PROTECTION = 0x9016U,
    TIS_REG_LOW_CHARGE_TEMPERATURE = 0x9017U,
    TIS_REG_LOW_DISCHARGE_TEMPERATURE = 0x9018U,
    TIS_REG_RTC_MINUTE_SECOND = 0x9019U,
    TIS_REG_RTC_DAY_HOUR = 0x901AU,
    TIS_REG_RTC_YEAR_MONTH = 0x901BU,
    TIS_REG_BATTERY_HIGH_TEMPERATURE = 0x901CU,
    TIS_REG_BATTERY_LOW_TEMPERATURE = 0x901DU,
    TIS_REG_DEVICE_HIGH_TEMPERATURE = 0x901EU,
    TIS_REG_DEVICE_HIGH_TEMPERATURE_RECOVER = 0x901FU,
    TIS_REG_POWER_HIGH_TEMPERATURE = 0x9020U,
    TIS_REG_POWER_HIGH_TEMPERATURE_RECOVER = 0x9021U,
    TIS_REG_LOAD_MODE = 0x9022U,
    TIS_REG_MANUAL_DEFAULT_SWITCH = 0x9023U,
    TIS_REG_LIGHT_ON_VOLTAGE = 0x9024U,
    TIS_REG_LIGHT_ON_DELAY = 0x9025U,
    TIS_REG_LIGHT_OFF_VOLTAGE = 0x9026U,
    TIS_REG_LIGHT_OFF_DELAY = 0x9027U,
    TIS_REG_NIGHT_DURATION = 0x9028U,
    TIS_REG_SELECTED_PERIOD = 0x9029U,
    TIS_REG_WORK_DURATION1 = 0x902AU,
    TIS_REG_WORK_DURATION2 = 0x902BU,
    TIS_REG_ON1_SECOND = 0x902CU,
    TIS_REG_ON1_MINUTE = 0x902DU,
    TIS_REG_ON1_HOUR = 0x902EU,
    TIS_REG_OFF1_SECOND = 0x902FU,
    TIS_REG_OFF1_MINUTE = 0x9030U,
    TIS_REG_OFF1_HOUR = 0x9031U,
    TIS_REG_ON2_SECOND = 0x9032U,
    TIS_REG_ON2_MINUTE = 0x9033U,
    TIS_REG_ON2_HOUR = 0x9034U,
    TIS_REG_OFF2_SECOND = 0x9035U,
    TIS_REG_OFF2_MINUTE = 0x9036U,
    TIS_REG_OFF2_HOUR = 0x9037U,
    TIS_REG_CHARGE_DISCHARGE_MODE = 0x9038U,
    TIS_REG_FULL_SOC = 0x9039U,
    TIS_REG_FULL_SOC_RECOVER = 0x903AU,
    TIS_REG_DISCHARGE_SOC_RECOVER = 0x903BU,
    TIS_REG_LOW_SOC_RECOVER = 0x903CU,
    TIS_REG_LOW_SOC = 0x903DU,
    TIS_REG_DISCHARGE_SOC = 0x903EU,
    TIS_REG_RECORD_INTERVAL = 0x903FU,
    TIS_REG_BMS_PROTOCOL_SELECT = 0x9040U,
    TIS_REG_BMS_PARAMETERS_ENABLED = 0x9041U,
    TIS_REG_PV_PARALLEL_MODE = 0x9042U,
    TIS_REG_SIMULATED_BMS = 0x9043U,
    TIS_REG_COMMUNICATION_ID = 0x9045U,
    TIS_REG_BAUDRATE = 0x9046U,
    TIS_REG_PARALLEL_CHARGE_CURRENT = 0x9047U,
    TIS_REG_PV_RESTART_PERIOD = 0x9049U,
    TIS_REG_COMMUNICATION_ENABLED = 0x904AU,
    TIS_REG_MANUAL_EQUALIZE = 0x904BU,
};

enum {
    TIS_COIL_CHARGE = 0,
    TIS_COIL_MANUAL_OUTPUT = 3,
    TIS_COIL_FACTORY_RESET = 7,
    TIS_COIL_CLEAR_ENERGY = 8, /* 设备要求 Root 权限，不提供认证绕过。 */
    TIS_COIL_FAULT_RESET = 9,
};

/* 状态字段保持原始字，按表定义掩码；多位状态先 & MASK 再 >> SHIFT。 */
#define TIS_BATTERY_VOLTAGE_MASK             0x000FU
#define TIS_BATTERY_TEMPERATURE_MASK         0x00F0U
#define TIS_BATTERY_TEMPERATURE_SHIFT        4U
#define TIS_BATTERY_CHARGE_PROTECTION        0x0200U
#define TIS_BATTERY_DISCHARGE_PROTECTION     0x0400U
#define TIS_LOAD_RUNNING                    0x0001U
#define TIS_LOAD_FAULT                      0x0002U
#define TIS_LOAD_SHORT_CIRCUIT              0x0800U
#define TIS_LOAD_POWER_MASK                 0x3000U
#define TIS_LOAD_POWER_SHIFT                12U
#define TIS_CHARGE_PV_RUNNING               0x0001U
#define TIS_CHARGE1_NIGHT                   0x0002U
#define TIS_CHARGE1_STAGE_MASK              0x000CU
#define TIS_CHARGE1_STAGE_SHIFT             2U
#define TIS_CHARGE_REVERSE_POLARITY          0x0010U
#define TIS_CHARGE1_DEVICE_OVERHEAT         0x0020U
#define TIS_CHARGE1_PHASE4_SHORT            0x0400U
#define TIS_CHARGE1_PHASE3_SHORT            0x0800U
#define TIS_CHARGE1_PHASE2_SHORT            0x1000U
#define TIS_CHARGE1_PHASE1_SHORT            0x2000U
#define TIS_CHARGE_INPUT_VOLTAGE_MASK        0xC000U
#define TIS_CHARGE_INPUT_VOLTAGE_SHIFT       14U
#define TIS_CHARGE2_MPPT                    0x0020U
#define TIS_CHARGE2_PV1_RELAY_FAULT          0x0040U
#define TIS_CHARGE2_PV2_RELAY_FAULT          0x0080U
#define TIS_CHARGE2_CONTACT_INHIBIT          0x0100U
#define TIS_CHARGE2_PLL_LOW_ENERGY           0x0200U
#define TIS_CHARGE2_PV_MODE_FAULT2           0x0400U
#define TIS_CHARGE2_PV_MODE_FAULT1           0x0800U
#define TIS_DEVICE_BMS_ONLINE                0x0001U
#define TIS_DEVICE_SOC_LOW                  0x0002U
#define TIS_DEVICE_BMS_DISCHARGE_PROTECT     0x0004U
#define TIS_DEVICE_BMS_CHARGE_PROTECT        0x0008U
#define TIS_DEVICE_BMS_OVERDISCHARGE         0x0010U
#define TIS_DEVICE_BMS_SENSOR_FAULT          0x0020U
#define TIS_DEVICE_CELL_COLD                 0x0040U
#define TIS_DEVICE_CELL_HOT                  0x0080U
#define TIS_DEVICE_CELL_UNDERVOLTAGE         0x0100U
#define TIS_DEVICE_CELL_OVERVOLTAGE          0x0200U
#define TIS_DEVICE_BMS_OVERCHARGE            0x0400U
#define TIS_DEVICE_BMS_OTHER_FAULT           0x0800U
#define TIS_DEVICE_SOC_DISCHARGE_OFF         0x1000U
#define TIS_DEVICE_SOC_FULL                 0x2000U
#define TIS_DEVICE_DSP_COMM_FAULT            0x4000U
#define TIS_DEVICE_DUPLICATE_PARALLEL_ID     0x8000U
#define TIS_BMS_MOS_CHARGING                 0x0001U
#define TIS_BMS_MOS_DISCHARGING              0x0002U
#define TIS_BMS_BATTERY_CHARGE_PROTECT       0x0001U
#define TIS_BMS_BATTERY_DISCHARGE_PROTECT    0x0002U
#define TIS_BMS_BATTERY_OTHER_PROTECT        0x0008U
#define TIS_BMS_BATTERY_CHARGE_OVERHEAT      0x0010U
#define TIS_BMS_BATTERY_DISCHARGE_OVERHEAT   0x0020U
#define TIS_BMS_BATTERY_FULL_REQUEST         0x1000U
#define TIS_BMS_BATTERY_FORCE_CHARGE         0x2000U
#define TIS_BMS_BATTERY_DISCHARGE_INHIBIT    0x4000U
#define TIS_BMS_BATTERY_CHARGE_INHIBIT       0x8000U

/* 字段保存定点原始量：工程值 = 字段 / 系数；无系数为枚举/打包值。
 * 无更新时间或分段有效标记；online 只表示最近事务状态。 */
typedef struct {
    uint8_t slave_addr;
    modbus_dev_status_t status;
    uint16_t rated_pv_voltage; /* 0x3002 PV最大输入电压 (V); 系数 100 */
    uint16_t rated_pv_current; /* 0x3003 PV最大输入电流 (A); 系数 100 */
    uint32_t rated_pv_power; /* 0x3004 PV输入额定功率L (W); 系数 100 */
    uint16_t rated_battery_voltage; /* 0x3006 蓄电池额定电压 (V); 系数 100 */
    uint16_t rated_charge_current; /* 0x3007 蓄电池额定充电电流 (A); 系数 100 */
    uint32_t rated_charge_power; /* 0x3008 蓄电池额定充电功率L (W); 系数 100 */
    uint16_t rated_load_voltage; /* 0x300A 负载额定电压 (V); 系数 100 */
    uint16_t rated_load_current; /* 0x300B 负载额定电流 (A); 系数 100 */
    uint32_t rated_load_power; /* 0x300C 负载额定功率L (W); 系数 100 */
    uint16_t pv1_input_voltage; /* 0x3100 PV1输入电压 (V); 系数 100 */
    uint16_t pv1_input_current; /* 0x3101 PV1输入电流 (A); 系数 100 */
    uint32_t pv1_input_power; /* 0x3102 PV1输入功率L (W); 系数 100 */
    uint16_t pv1_output_voltage; /* 0x3104 PV1输出电压 (V); 系数 100 */
    uint16_t pv1_output_current; /* 0x3105 PV1输出电流 (A); 系数 100 */
    uint32_t pv1_output_power; /* 0x3106 PV1输出功率L (W); 系数 100 */
    uint16_t pv2_input_voltage; /* 0x3108 PV2输入电压 (V); 系数 100 */
    uint16_t pv2_input_current; /* 0x3109 PV2输入电流 (A); 系数 100 */
    uint32_t pv2_input_power; /* 0x310A PV2输入功率L (W); 系数 100 */
    uint16_t pv2_output_voltage; /* 0x310C PV2输出电压 (V); 系数 100 */
    uint16_t pv2_output_current; /* 0x310D PV2输出电流 (A); 系数 100 */
    uint32_t pv2_output_power; /* 0x310E PV2输出功率L (W); 系数 100 */
    uint16_t load_voltage; /* 0x3110 负载输出电压 (V); 系数 100 */
    uint16_t load_current; /* 0x3111 负载输出电流 (A); 系数 100 */
    uint32_t load_power; /* 0x3112 负载输出功率L (W); 系数 100 */
    uint16_t battery_voltage; /* 0x3114 蓄电池电压 (V); 系数 100 */
    uint16_t battery_charge_current; /* 0x3115 蓄电池总充电电流 (A); 系数 100 */
    uint16_t battery_discharge_current; /* 0x3116 蓄电池总放电电流 (A); 系数 100 */
    uint16_t battery_total_current; /* 0x3117 蓄电池总电流 (A); 系数 100 */
    int16_t battery_temperature; /* 0x3118 远程蓄电池温度 (°C); 系数 100 */
    uint16_t battery_soc; /* 0x3119 蓄电池SOC (%); 系数 100 */
    int16_t device_temperature; /* 0x311A 设备温度 (°C); 系数 100 */
    uint16_t system_voltage; /* 0x311D 当前系统电压 (V); 系数 100 */
    uint16_t max_pv_voltage; /* 0x311E 2路输入中较高的PV电压 (V); 系数 100 */
    uint16_t pv_charge_current; /* 0x311F PV总充电电流 (A); 系数 100 */
    uint32_t pv_charge_power; /* 0x3120 PV总充电功率L (W); 系数 100 */
    uint16_t battery_state; /* 0x3200 蓄电池状态; 系数 1 */
    uint16_t load_state; /* 0x3201 负载状态; 系数 1 */
    uint16_t charge1_state; /* 0x3202 充电状态1; 系数 1 */
    uint16_t charge2_state; /* 0x3203 充电状态2; 系数 1 */
    uint16_t device_state; /* 0x3205 设备状态; 系数 1 */
    uint16_t today_max_pv_voltage; /* 0x3300 今日PV最高输入电压 (V); 系数 100 */
    uint16_t today_max_battery_voltage; /* 0x3301 今日最高蓄电池电压 (V); 系数 100 */
    uint16_t today_min_battery_voltage; /* 0x3302 今日最低蓄电池电压 (V); 系数 100 */
    uint32_t today_consumption; /* 0x3303 今日总用电电量L (Kwh); 系数 100 */
    uint32_t month_consumption; /* 0x3305 本月总用电电量L (Kwh); 系数 100 */
    uint32_t year_consumption; /* 0x3307 本年总用电电量L (Kwh); 系数 100 */
    uint32_t total_consumption; /* 0x3309 总累计用电电量L (Kwh); 系数 100 */
    uint32_t today_generation; /* 0x330B 今日总发电电量L (Kwh); 系数 100 */
    uint32_t month_generation; /* 0x330D 本月总发电电量L (Kwh); 系数 100 */
    uint32_t year_generation; /* 0x330F 本年总发电电量L (Kwh); 系数 100 */
    uint32_t total_generation; /* 0x3311 总累计发电电量L (Kwh); 系数 100 */
    uint32_t learned_capacity; /* 0x3313 学习的电池容量L (Ah); 系数 1 */
    uint16_t bms_cell_count; /* 0x3400 电芯节数 (节); 系数 1 */
    uint16_t bms_voltage; /* 0x3401 电池包电压 (V); 系数 100 */
    uint16_t bms_current; /* 0x3402 电池包主回路电流 (A); 系数 100; 表内称可为负，按类型栏保留无符号原始值 */
    uint32_t bms_power; /* 0x3403 电池包功率L (W); 系数 100 */
    uint16_t bms_full_capacity; /* 0x3405 电池包充满容量 (AH); 系数 1 */
    uint16_t bms_soc; /* 0x3406 电池包剩余容量百分比 (%); 系数 1 */
    uint16_t bms_remaining_minutes; /* 0x3407 电池包剩余工作时间 (M); 系数 1 */
    int16_t bms_max_cell_temperature; /* 0x3408 电池包电芯温度最大值 (°C); 系数 100 */
    int16_t bms_min_cell_temperature; /* 0x3409 电池包电芯温度最小值 (°C); 系数 100 */
    int16_t bms_balance_temperature; /* 0x340A 均衡温度 (°C); 系数 100 */
    int16_t bms_ambient_temperature; /* 0x340B 环境温度 (°C); 系数 100 */
    int16_t bms_mos_temperature; /* 0x340C MOS管温度 (°C); 系数 100 */
    uint16_t bms_cycle_count; /* 0x340D 循环次数 (次); 系数 1 */
    uint16_t bms_balance_state; /* 0x340E 均衡标志; 系数 1 */
    uint16_t bms_voltage_state; /* 0x340F 电池包电压状态; 系数 1 */
    uint16_t bms_current_state; /* 0x3410 电流状态; 系数 1 */
    uint16_t bms_mos_state; /* 0x3411 MOS状态; 系数 1 */
    uint16_t bms_cell_temperature_state; /* 0x3412 电芯温度状态; 系数 1 */
    uint16_t bms_balance_temperature_state; /* 0x3413 均衡温度状态; 系数 1 */
    uint16_t bms_ambient_temperature_state; /* 0x3414 环境温度状态; 系数 1 */
    uint16_t bms_mos_temperature_state; /* 0x3415 MOS管温度状态; 系数 1 */
    uint16_t bms_cell_state; /* 0x3416 电芯状态; 系数 1 */
    uint16_t bms_protocol; /* 0x3417 电池协议; 系数 1 */
    uint16_t bms_battery_state; /* 0x3418 电池状态; 系数 1 */
    uint16_t bms_pack_mode; /* 0x3419 电池包状态; 系数 1 */
    uint16_t bms_group_voltage; /* 0x341A 电池包电压 (V); 系数 10 */
    uint16_t bms_group_current; /* 0x341B 电池包电流 (A); 系数 10; 表内称可为负，按类型栏保留无符号原始值 */
    uint16_t bms_undervoltage_limit; /* 0x341C 电池包欠压保护值 (V); 系数 100 */
    uint16_t bms_overvoltage_alarm; /* 0x341D 电池包过压告警值 (V); 系数 100 */
    uint16_t bms_charge_current_limit; /* 0x341E 充电电流限流值 (A); 系数 100 */
    uint16_t bms_discharge_current_limit; /* 0x341F 放电电流限流值 (A); 系数 100 */
    uint16_t bms_group_undervoltage_limit; /* 0x3420 电池组欠压保护值 (V); 系数 100 */
    uint16_t bms_group_overvoltage_limit; /* 0x3421 电池组过压告警值 (V); 系数 100 */
    uint16_t bms_group_charge_current_limit; /* 0x3422 电池组充电电流保护值 (A); 系数 100 */
    uint16_t bms_group_discharge_current_limit; /* 0x3423 电池组放电电流保护值 (A); 系数 100 */
    uint16_t battery_type; /* 0x9000 蓄电池类型; 系数 1 */
    uint16_t battery_capacity; /* 0x9001 蓄电池容量 (AH); 系数 1 */
    uint16_t temperature_compensation; /* 0x9002 温度补偿系数 (mV/°C/2V); 系数 100 */
    uint16_t temperature_unit; /* 0x9003 温度单位; 系数 1 */
    uint16_t backlight_seconds; /* 0x9004 屏幕背光时间 (S); 系数 1 */
    uint16_t display_cycle_seconds; /* 0x9005 屏幕循环时间 (S); 系数 1 */
    int16_t voltage_level; /* 0x9006 额定电压等级; 系数 无（枚举/打包） */
    uint16_t overvoltage_disconnect; /* 0x9007 过压断开电压 (V); 系数 100 */
    uint16_t charge_voltage_limit; /* 0x9008 充电限制电压 (V); 系数 100 */
    uint16_t overvoltage_reconnect; /* 0x9009 过压断开恢复电压 (V); 系数 100 */
    int16_t equalize_voltage; /* 0x900A 均衡电压 (V); 系数 100 */
    uint16_t boost_voltage; /* 0x900B 提升电压 (V); 系数 100 */
    uint16_t float_voltage; /* 0x900C 浮充电压 (V); 系数 100 */
    uint16_t boost_reconnect_voltage; /* 0x900D 提升恢复电压 (V); 系数 100 */
    int16_t low_voltage_reconnect; /* 0x900E 低压断开恢复电压 (V); 系数 100 */
    uint16_t undervoltage_alarm_reconnect; /* 0x900F 欠压报警恢复电压 (V); 系数 100 */
    uint16_t undervoltage_alarm; /* 0x9010 欠压报警电压 (V); 系数 100 */
    uint16_t low_voltage_disconnect; /* 0x9011 低压断开电压 (V); 系数 100 */
    uint16_t discharge_voltage_limit; /* 0x9012 放电限制电压 (V); 系数 100 */
    uint16_t allowed_charge_current; /* 0x9013 允许充电电流 (A); 系数 100 */
    uint16_t equalize_minutes; /* 0x9014 均衡充电时间 (M); 系数 1 */
    uint16_t boost_minutes; /* 0x9015 提升充电时间 (M); 系数 1 */
    uint16_t lithium_protection; /* 0x9016 锂电池保护使能; 系数 1 */
    int16_t low_charge_temperature; /* 0x9017 低温禁止充电温度 (°C); 系数 100 */
    int16_t low_discharge_temperature; /* 0x9018 低温禁止放电温度 (°C); 系数 100 */
    uint16_t rtc_minute_second; /* 0x9019 实时时钟; 系数 无（枚举/打包） */
    uint16_t rtc_day_hour; /* 0x901A 实时时钟; 系数 无（枚举/打包） */
    uint16_t rtc_year_month; /* 0x901B 实时时钟; 系数 无（枚举/打包） */
    uint16_t battery_high_temperature; /* 0x901C 蓄电池温度告警上限值 (°C); 系数 100 */
    int16_t battery_low_temperature; /* 0x901D 蓄电池温度告警下限值 (°C); 系数 100 */
    uint16_t device_high_temperature; /* 0x901E 设备内温度上限 (°C); 系数 100 */
    uint16_t device_high_temperature_recover; /* 0x901F 设备内温度上限恢复 (°C); 系数 100 */
    uint16_t power_high_temperature; /* 0x9020 功率器件温度上限 (°C); 系数 100 */
    uint16_t power_high_temperature_recover; /* 0x9021 功率器件温度上限恢复 (°C); 系数 100 */
    uint16_t load_mode; /* 0x9022 负载控制模式; 系数 1 */
    uint16_t manual_default_switch; /* 0x9023 手动模式默认开关; 系数 1 */
    uint16_t light_on_voltage; /* 0x9024 光控启动电压 (V); 系数 100 */
    uint16_t light_on_delay; /* 0x9025 光控启动延时 (M); 系数 1 */
    uint16_t light_off_voltage; /* 0x9026 光控关闭电压 (V); 系数 0.01 */
    uint16_t light_off_delay; /* 0x9027 光控关闭延时 (M); 系数 1 */
    uint16_t night_duration; /* 0x9028 夜间时长; 系数 1 */
    uint16_t selected_period; /* 0x9029 定时控制时间段; 系数 1 */
    uint16_t work_duration1; /* 0x902A 工作时长 1; 系数 1 */
    uint16_t work_duration2; /* 0x902B 工作时长 2; 系数 1 */
    uint16_t on1_second; /* 0x902C 定时开时刻 1 (S); 系数 1 */
    uint16_t on1_minute; /* 0x902D  (M); 系数 1 */
    uint16_t on1_hour; /* 0x902E  (H); 系数 1 */
    uint16_t off1_second; /* 0x902F 定时关时刻 1 (S); 系数 1 */
    uint16_t off1_minute; /* 0x9030  (M); 系数 1 */
    uint16_t off1_hour; /* 0x9031  (H); 系数 1 */
    uint16_t on2_second; /* 0x9032 定时开时刻 2 (S); 系数 1 */
    uint16_t on2_minute; /* 0x9033  (M); 系数 1 */
    uint16_t on2_hour; /* 0x9034  (H); 系数 1 */
    uint16_t off2_second; /* 0x9035 定时关时刻 2 (S); 系数 1 */
    uint16_t off2_minute; /* 0x9036  (M); 系数 1 */
    uint16_t off2_hour; /* 0x9037  (H); 系数 1 */
    uint16_t charge_discharge_mode; /* 0x9038 充放电模式; 系数 1 */
    uint16_t full_soc; /* 0x9039 充满保护SOC (%); 系数 1 */
    uint16_t full_soc_recover; /* 0x903A 充满保护恢复SOC (%); 系数 1 */
    uint16_t discharge_soc_recover; /* 0x903B 放电保护恢复SOC (%); 系数 1 */
    uint16_t low_soc_recover; /* 0x903C 低电量告警恢复SOC (%); 系数 1 */
    uint16_t low_soc; /* 0x903D 低电量告警SOC (%); 系数 1 */
    uint16_t discharge_soc; /* 0x903E 放电保护SOC (%); 系数 1 */
    uint16_t record_interval; /* 0x903F 存储间隔时间 (M); 系数 1 */
    uint16_t bms_protocol_select; /* 0x9040 BMS协议选择; 系数 1 */
    uint16_t bms_parameters_enabled; /* 0x9041 BMS控制参数有效; 系数 1 */
    uint16_t pv_parallel_mode; /* 0x9042 PV并联模式; 系数 1 */
    uint16_t simulated_bms; /* 0x9043 模拟BMS模式; 系数 1 */
    uint16_t communication_id; /* 0x9045 通信ID号; 系数 1 */
    uint16_t baudrate; /* 0x9046 波特率; 系数 0.01 */
    uint16_t parallel_charge_current; /* 0x9047 并联允许充电电流 (A); 系数 1 */
    uint16_t pv_restart_period; /* 0x9049 PV充电重启周期 (M); 系数 1 */
    uint16_t communication_enabled; /* 0x904A 通讯口使能; 系数 1 */
    uint16_t manual_equalize; /* 0x904B 手动均衡; 系数 1 */
    bool charging_enabled;
    bool manual_output_enabled;
    bool factory_reset;
    bool clear_energy;
    bool fault_reset;
} tis_device_t;

/* 每次 <=16 寄存器/位，不跨空洞，不拆开 32 位或成组时间字段。 */
uint32_t modbus_tis_build_read_request(uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity);
/* values 为线路原始值，负 int16 转为 uint16_t 补码传入。
 * 写电压保护点时 voltage_level 必须为实际 12V 倍数 1..4；其他写入可传 0。
 * 不依赖未刷新缓存推断电压等级；表未提供的约束不自行补充。 */
uint32_t modbus_tis_build_write_params(uint8_t slave_addr, uint16_t start, const uint16_t *values, uint16_t count, uint16_t voltage_level, uint8_t *frame, uint32_t capacity);
uint32_t modbus_tis_build_write_coil(uint8_t slave_addr, uint16_t coil, bool on, uint8_t *frame, uint32_t capacity);
modbus_rtu_status_t modbus_tis_parse_read(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, tis_device_t *out);

#endif
