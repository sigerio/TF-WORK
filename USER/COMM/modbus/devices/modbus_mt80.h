#ifndef MODBUS_MT80_H
#define MODBUS_MT80_H

#include "COMM/modbus/devices/modbus_common.h"
#include "COMM/modbus/modbus_config.h"

#define MT80_REGISTER_COUNT 81U
#define MT80_COIL_COUNT 5U

#define MT80_WRITE_FRAME_MAX (9U + 2U * MODULE_MODBUS_MT80_MAX_REGS)

/* MT80 寄存器表。 */
enum {
    MT80_REG_RATED_INVERTER_INPUT_VOLTAGE = 0x340AU,
    MT80_REG_RATED_INVERTER_POWER_L = 0x3410U,
    MT80_REG_RATED_PV_VOLTAGE = 0x3416U,
    MT80_REG_RATED_PV_POWER_L = 0x3418U,
    MT80_REG_RATED_PV_OUTPUT_VOLTAGE = 0x341AU,
    MT80_REG_RATED_PV_OUTPUT_CURRENT = 0x341BU,
    MT80_REG_INVERTER_VOLTAGE = 0x3521U,
    MT80_REG_INVERTER_CURRENT = 0x3522U,
    MT80_REG_INVERTER_STATE = 0x3523U,
    MT80_REG_INVERTER_POWER_L = 0x3524U,
    MT80_REG_INVERTER_FREQUENCY = 0x3529U,
    MT80_REG_INVERTER_DAY_ENERGY_L = 0x352AU,
    MT80_REG_INVERTER_MONTH_ENERGY_L = 0x352CU,
    MT80_REG_INVERTER_YEAR_ENERGY_L = 0x352EU,
    MT80_REG_INVERTER_TOTAL_ENERGY_L = 0x3530U,
    MT80_REG_PV1_INPUT_VOLTAGE = 0x3549U,
    MT80_REG_PV1_INPUT_CURRENT = 0x354AU,
    MT80_REG_PV1_INPUT_POWER_L = 0x354BU,
    MT80_REG_TODAY_GENERATION_L = 0x3551U,
    MT80_REG_MONTH_GENERATION_L = 0x3553U,
    MT80_REG_YEAR_GENERATION_L = 0x3555U,
    MT80_REG_TOTAL_GENERATION_L = 0x3557U,
    MT80_REG_CHARGE1_STATE = 0x3559U,
    MT80_REG_LOAD_VOLTAGE = 0x3560U,
    MT80_REG_LOAD_CURRENT = 0x3561U,
    MT80_REG_LOAD_POWER_L = 0x3562U,
    MT80_REG_LOAD_STATE = 0x3564U,
    MT80_REG_TODAY_CONSUMPTION_L = 0x3565U,
    MT80_REG_MONTH_CONSUMPTION_L = 0x3567U,
    MT80_REG_YEAR_CONSUMPTION_L = 0x3569U,
    MT80_REG_TOTAL_CONSUMPTION_L = 0x356BU,
    MT80_REG_BATTERY_TEMPERATURE_RAW = 0x356FU,
    MT80_REG_BATTERY_VOLTAGE = 0x3580U,
    MT80_REG_BATTERY_CURRENT_L = 0x3581U,
    MT80_REG_BATTERY_SOC = 0x3586U,
    MT80_REG_BATTERY_STATE = 0x3589U,
    MT80_REG_BATTERY_TYPE = 0x960FU,
    MT80_REG_BATTERY_CAPACITY = 0x9610U,
    MT80_REG_TEMPERATURE_COMPENSATION = 0x9611U,
    MT80_REG_EQUALIZE_MINUTES = 0x9613U,
    MT80_REG_BOOST_MINUTES = 0x9614U,
    MT80_REG_OVERVOLTAGE_DISCONNECT = 0x9615U,
    MT80_REG_CHARGE_VOLTAGE_LIMIT = 0x9616U,
    MT80_REG_OVERVOLTAGE_RECONNECT = 0x9617U,
    MT80_REG_EQUALIZE_VOLTAGE = 0x9618U,
    MT80_REG_BOOST_VOLTAGE = 0x9619U,
    MT80_REG_FLOAT_VOLTAGE = 0x961AU,
    MT80_REG_BOOST_RECONNECT_VOLTAGE = 0x961BU,
    MT80_REG_LOW_VOLTAGE_RECONNECT = 0x961CU,
    MT80_REG_UNDERVOLTAGE_ALARM_RECONNECT = 0x961DU,
    MT80_REG_UNDERVOLTAGE_ALARM = 0x961EU,
    MT80_REG_LOW_VOLTAGE_DISCONNECT = 0x961FU,
    MT80_REG_DISCHARGE_VOLTAGE_LIMIT = 0x9620U,
    MT80_REG_LITHIUM_PROTECTION = 0x9621U,
    MT80_REG_LOW_CHARGE_TEMPERATURE_RAW = 0x9622U,
    MT80_REG_LOW_DISCHARGE_TEMPERATURE_RAW = 0x9623U,
    MT80_REG_RTC_MINUTE_SECOND = 0x9624U,
    MT80_REG_INVERTER_VOLTAGE_SETTING = 0x9631U,
    MT80_REG_INVERTER_FREQUENCY_SETTING = 0x9632U,
    MT80_REG_INVERTER_LOW_VOLTAGE_DISCONNECT = 0x9633U,
    MT80_REG_INVERTER_LOW_VOLTAGE_RECONNECT = 0x9634U,
    MT80_REG_INVERTER_OVERVOLTAGE_RECONNECT = 0x9635U,
    MT80_REG_INVERTER_OVERVOLTAGE_DISCONNECT = 0x9636U,
    MT80_REG_BATTERY_HIGH_TEMPERATURE_RAW = 0x9656U,
    MT80_REG_BATTERY_LOW_TEMPERATURE_RAW = 0x9657U,
    MT80_REG_DEVICE_HIGH_TEMPERATURE_RAW = 0x9658U,
    MT80_REG_DEVICE_HIGH_TEMPERATURE_RECOVER_RAW = 0x9659U,
    MT80_REG_LIGHT_ON_VOLTAGE = 0x965CU,
    MT80_REG_LIGHT_ON_DELAY = 0x965DU,
    MT80_REG_LIGHT_OFF_VOLTAGE = 0x965EU,
    MT80_REG_LIGHT_OFF_DELAY = 0x965FU,
    MT80_REG_LOAD_MODE = 0x9660U,
    MT80_REG_WORK_DURATION1 = 0x9661U,
    MT80_REG_WORK_DURATION2 = 0x9662U,
    MT80_REG_LOAD_ON_TIME1 = 0x9663U,
    MT80_REG_LOAD_OFF_TIME1 = 0x9666U,
    MT80_REG_LOAD_ON_TIME2 = 0x9669U,
    MT80_REG_LOAD_OFF_TIME2 = 0x966CU,
    MT80_REG_NIGHT_DURATION = 0x966FU,
    MT80_REG_SELECTED_PERIOD = 0x9670U,
    MT80_REG_MANUAL_DEFAULT_SWITCH = 0x9671U,
};

enum {
    MT80_COIL_CLEAR_ENERGY = 0x0100U,
    MT80_COIL_FAULT_RESET = 0x0101U,
    MT80_COIL_INVERTER_OUTPUT = 0x0106U,
    MT80_COIL_REMOTE_CONTROL = 0x0108U,
    MT80_COIL_DC_OUTPUT = 0x010DU,
};

#define MT80_BATTERY_VOLTAGE_MASK         0x000FU
#define MT80_BATTERY_TEMPERATURE_MASK     0x00F0U
#define MT80_BATTERY_TEMPERATURE_SHIFT    4U
#define MT80_BATTERY_RESISTANCE_FAULT     0x0100U
#define MT80_BATTERY_CHARGE_PROTECTION    0x0200U
#define MT80_BATTERY_DISCHARGE_PROTECTION 0x0400U
#define MT80_BATTERY_RATED_VOLTAGE_ERROR  0x8000U
#define MT80_LOAD_RUNNING                0x0001U
#define MT80_LOAD_FAULT                  0x0002U
#define MT80_LOAD_SHORT_CIRCUIT          0x0800U
#define MT80_LOAD_POWER_MASK             0x3000U
#define MT80_LOAD_POWER_SHIFT            12U
#define MT80_CHARGE_PV_RUNNING           0x0001U
#define MT80_CHARGE1_STAGE_MASK          0x000CU
#define MT80_CHARGE1_STAGE_SHIFT         2U
#define MT80_CHARGE_PV_SHORT             0x0010U
#define MT80_CHARGE_INPUT_VOLTAGE_MASK   0xC000U
#define MT80_CHARGE_INPUT_VOLTAGE_SHIFT  14U
/* MT80 的充电状态 D1、非 LED 型 D5 不解析；其余位保留原始字。 */

/* 电压/电流/功率/电量按 100 倍原码保存；SOC 为整数百分比。
 * L/H 按低字在前组合；有符号量保留补码位模式。
 * 四组定时时刻按秒/分/时三寄存器读取，缓存为当天累计秒数。
 * online 仅表示最近事务状态，不代表所有分段均已刷新。 */
typedef struct {
    uint8_t slave_addr;
    modbus_dev_status_t status;
    uint8_t register_valid[MT80_REGISTER_COUNT];
    uint8_t coil_valid;
    uint16_t rated_inverter_input_voltage; /* 0x340A 逆变放电设备额定输入电压 */
    uint32_t rated_inverter_power; /* 0x3410 逆变放电设备额定输出视在功率L */
    uint16_t rated_pv_voltage; /* 0x3416 光伏阵列充电设备额定输入电压 */
    uint32_t rated_pv_power; /* 0x3418 光伏阵列充电设备额定输入功率L */
    uint16_t rated_pv_output_voltage; /* 0x341A 光伏阵列充电设备额定输出电压 */
    uint16_t rated_pv_output_current; /* 0x341B 光伏阵列充电设备额定输出电流 */
    uint16_t inverter_voltage; /* 0x3521 逆变放电设备输出电压 */
    uint16_t inverter_current; /* 0x3522 逆变放电设备输出电流 */
    uint16_t inverter_state; /* 0x3523 逆变放电设备输出状态 */
    uint32_t inverter_power; /* 0x3524 逆变放电设备输出视在功率L */
    uint16_t inverter_frequency; /* 0x3529 逆变放电设备输出频率 */
    uint32_t inverter_day_energy; /* 0x352A 逆变放电设备日累计用电电量L */
    uint32_t inverter_month_energy; /* 0x352C 逆变放电设备月累计用电电量L */
    uint32_t inverter_year_energy; /* 0x352E 逆变放电设备年累计用电电量L */
    uint32_t inverter_total_energy; /* 0x3530 逆变放电设备总累计用电电量L */
    uint16_t pv1_input_voltage; /* 0x3549 阵列1输入电压 */
    uint16_t pv1_input_current; /* 0x354A 阵列1输入电流 */
    uint32_t pv1_input_power; /* 0x354B 阵列1输入功率L */
    uint32_t today_generation; /* 0x3551 阵列1日累计充电电量L */
    uint32_t month_generation; /* 0x3553 阵列1月累计充电电量L */
    uint32_t year_generation; /* 0x3555 阵列1年累计充电电量L */
    uint32_t total_generation; /* 0x3557 阵列1总累计充电电量L */
    uint16_t charge1_state; /* 0x3559 光伏阵列1充电设备1工作状态 */
    uint16_t load_voltage; /* 0x3560 光伏阵列1放电设备输出电压 */
    uint16_t load_current; /* 0x3561 光伏阵列1放电设备输出电流 */
    uint32_t load_power; /* 0x3562 光伏阵列1放电设备输出功率L */
    uint16_t load_state; /* 0x3564 光伏阵列1放电设备1工作状态 */
    uint32_t today_consumption; /* 0x3565 光伏阵列1当日累计用电量L */
    uint32_t month_consumption; /* 0x3567 光伏阵列1当月累计用电量L */
    uint32_t year_consumption; /* 0x3569 光伏阵列1当年累计用电量L */
    uint32_t total_consumption; /* 0x356B 光伏阵列1总累计用电量L */
    uint16_t battery_temperature_raw; /* 0x356F 光伏阵列1设备自测蓄电池温度 */
    uint16_t battery_voltage; /* 0x3580 蓄电池1电压 */
    uint32_t battery_current; /* 0x3581 蓄电池1电流L */
    uint16_t battery_soc; /* 0x3586 蓄电池1容量（SOC） */
    uint16_t battery_state; /* 0x3589 蓄电池1状态 */
    uint16_t battery_type; /* 0x960F 蓄电池类型 */
    uint16_t battery_capacity; /* 0x9610 蓄电池容量 */
    uint16_t temperature_compensation; /* 0x9611 蓄电池温度补偿系数 */
    uint16_t equalize_minutes; /* 0x9613 充电均衡维持时间 */
    uint16_t boost_minutes; /* 0x9614 充电提升维持时间 */
    uint16_t overvoltage_disconnect; /* 0x9615 充电超压断开电压 */
    uint16_t charge_voltage_limit; /* 0x9616 充电充电限制电压 */
    uint16_t overvoltage_reconnect; /* 0x9617 充电超压断开恢复电压 */
    uint16_t equalize_voltage; /* 0x9618 充电均衡电压 */
    uint16_t boost_voltage; /* 0x9619 充电提升电压 */
    uint16_t float_voltage; /* 0x961A 充电浮充电压 */
    uint16_t boost_reconnect_voltage; /* 0x961B 充电提升恢复电压 */
    uint16_t low_voltage_reconnect; /* 0x961C 充电低压断开恢复电压 */
    uint16_t undervoltage_alarm_reconnect; /* 0x961D 充电欠压报警恢复电压 */
    uint16_t undervoltage_alarm; /* 0x961E 充电欠压报警电压 */
    uint16_t low_voltage_disconnect; /* 0x961F 充电低压断开电压 */
    uint16_t discharge_voltage_limit; /* 0x9620 充电放电限制电压 */
    uint16_t lithium_protection; /* 0x9621 锂电池参数使能 */
    uint16_t low_charge_temperature_raw; /* 0x9622 锂电池低温禁止充电控制点 */
    uint16_t low_discharge_temperature_raw; /* 0x9623 锂电池低温禁止放电控制点 */
    uint16_t rtc_minute_second; /* 0x9624 系统充电实时时钟 */
    uint16_t inverter_voltage_setting; /* 0x9631 逆变器输出电压等级 */
    uint16_t inverter_frequency_setting; /* 0x9632 逆变器输出频率 */
    uint16_t inverter_low_voltage_disconnect; /* 0x9633 DC-AC模块低压断开电压值 */
    uint16_t inverter_low_voltage_reconnect; /* 0x9634 DC-AC模块低压断开恢复电压值 */
    uint16_t inverter_overvoltage_reconnect; /* 0x9635 DC-AC模块过压断开恢复电压值 */
    uint16_t inverter_overvoltage_disconnect; /* 0x9636 DC-AC模块过压断开电压值 */
    uint16_t battery_high_temperature_raw; /* 0x9656 太阳能充电设备蓄电池1温度告警上限值 */
    uint16_t battery_low_temperature_raw; /* 0x9657 太阳能充电设备蓄电池1温度告警下限值 */
    uint16_t device_high_temperature_raw; /* 0x9658 太阳能充电设备内温度上限 */
    uint16_t device_high_temperature_recover_raw; /* 0x9659 太阳能充电设备内温度上限恢复 */
    uint16_t light_on_voltage; /* 0x965C 太阳能充电设备夜晚时间阈值电压 */
    uint16_t light_on_delay; /* 0x965D 太阳能充电设备光信号启动（夜晚）确认时间 */
    uint16_t light_off_voltage; /* 0x965E 太阳能充电设备白天时间阈值电压 */
    uint16_t light_off_delay; /* 0x965F 太阳能充电设备光信号关闭（白天）确认时间 */
    uint16_t load_mode; /* 0x9660 太阳能充电设备负载输出控制模式 */
    uint16_t work_duration1; /* 0x9661 太阳能充电设备工作时长 1 */
    uint16_t work_duration2; /* 0x9662 太阳能充电设备工作时长 2 */
    uint32_t load_on_time1; /* 0x9663 太阳能充电设备定时开时刻 1 */
    uint32_t load_off_time1; /* 0x9666 太阳能充电设备定时关时刻 1 */
    uint32_t load_on_time2; /* 0x9669 太阳能充电设备定时开时刻 2 */
    uint32_t load_off_time2; /* 0x966C 太阳能充电设备定时关时刻 2 */
    uint16_t night_duration; /* 0x966F 太阳能充电设备夜间时长 */
    uint16_t selected_period; /* 0x9670 太阳能充电设备定时控制时间段 */
    uint16_t manual_default_switch; /* 0x9671 太阳能充电设备手动控制条件下设定的开/关 */
    bool clear_energy; /* 0x0100 清除发电用电统计量 */
    bool fault_reset; /* 0x0101 故障清除 */
    bool inverter_output; /* 0x0106 负载输出开关 */
    bool remote_control; /* 0x0108 本地/远程控制 */
    bool dc_output; /* 0x010D 直流负载开关 */
} mt80_device_t;

typedef struct {
    uint16_t address;
    uint16_t offset;
    uint8_t divisor; /* 工程量倍率，INT/当天累计秒数为 1。 */
    uint8_t words; /* 1=16 位，2=L/H 双字，3=秒/分/时（缓存 uint32 秒数）。 */
    uint8_t is_signed; /* 按完整位宽解码二进制补码。 */
} mt80_register_t;

extern const mt80_register_t mt80_registers[MT80_REGISTER_COUNT];
extern const uint16_t mt80_coils[MT80_COIL_COUNT];

uint32_t modbus_mt80_build_read_request(uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_mt80_build_write_params(uint8_t slave_addr, uint16_t start, const uint16_t *values, uint16_t count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_mt80_build_write_coil(uint8_t slave_addr, uint16_t coil, bool on, uint8_t *frame, uint32_t capacity);
modbus_rtu_status_t modbus_mt80_parse_read(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, mt80_device_t *out);

#endif /* MODBUS_MT80_H */
