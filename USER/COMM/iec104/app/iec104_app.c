#include "COMM/iec104/app/iec104_app.h"

#include "COMM/modbus/modbus_devices.h"
#include <string.h>

#include "CONTROL/key/key_input.h"
#include "CONTROL/output/aux_output.h"
#include "CONTROL/motor/motor_control.h"
#include "APP/ws2812_app.h"

#define IEC104_QUALITY_IV 0x80U
#define IEC104_QUALITY_NT 0x40U
#define IEC104_QUALITY_SB 0x20U

/* 工程点表：所有 IOA 均按 3 字节小端编码；点表与数值编码见 iec104_protocol_spec.md。 */
/* 本机遥信 YX：门/平台状态、急停、灯光输出和门加热解冻使能状态。 */
#define IEC104_IOA_YX_DOOR_OPENED       0x000001UL /* 门打开到位。 */
#define IEC104_IOA_YX_DOOR_CLOSED       0x000002UL /* 门关闭到位。 */
#define IEC104_IOA_YX_HELIPED_EXTENDED  0x000003UL /* 停机坪伸出到位。 */
#define IEC104_IOA_YX_HELIPED_RETRACTED 0x000004UL /* 停机坪收回到位。 */
#define IEC104_IOA_YX_EM_STOP           0x000010UL /* 急停触发。 */
#define IEC104_IOA_YX_DOOR_RUNNING      0x000011UL /* 门运行中。 */
#define IEC104_IOA_YX_HELIPED_RUNNING   0x000012UL /* 停机坪运行中。 */
#define IEC104_IOA_YX_WS2812_EFFECT_ACTIVE 0x000013UL /* WS2812 门/平台灯效活动。 */
#define IEC104_IOA_YX_INDICATOR_LIGHT_ON   0x000014UL /* PC3 指示灯点亮。 */
#define IEC104_IOA_YX_INTERIOR_LIGHT_ON    0x000015UL /* PC4 内部照明点亮。 */
#define IEC104_IOA_YX_DOOR_HEATER_ENABLED  0x000016UL /* PC5 门加热解冻输出使能。 */

/* 本机遥测 YC：门和停机坪状态编码。 */
#define IEC104_IOA_YC_DOOR_STATE    0x000100UL /* 门状态码。 */
#define IEC104_IOA_YC_HELIPED_STATE 0x000101UL /* 停机坪状态码。 */

/* 遥控 YK：门、停机坪、空调、内部照明和门加热解冻命令。 */
#define IEC104_IOA_YK_DOOR_OPEN       0x000200UL /* 门打开。 */
#define IEC104_IOA_YK_DOOR_CLOSE      0x000201UL /* 门关闭。 */
#define IEC104_IOA_YK_HELIPED_EXTEND  0x000210UL /* 停机坪伸出。 */
#define IEC104_IOA_YK_HELIPED_RETRACT 0x000211UL /* 停机坪收回。 */
#define IEC104_IOA_YK_485_AC_POWER_ON  0x000220UL /* 空调开机。 */
#define IEC104_IOA_YK_485_AC_POWER_OFF 0x000221UL /* 空调关机。 */
#define IEC104_IOA_YK_INTERIOR_LIGHT_ON   0x000222UL /* 打开 PC4 内部照明。 */
#define IEC104_IOA_YK_INTERIOR_LIGHT_OFF  0x000223UL /* 关闭 PC4 内部照明。 */
#define IEC104_IOA_YK_DOOR_HEATER_ENABLE  0x000224UL /* 使能 PC5 门加热解冻。 */
#define IEC104_IOA_YK_DOOR_HEATER_DISABLE 0x000225UL /* 关闭 PC5 门加热输出。 */

/* RS-485 子设备遥信 YX。 */
#define IEC104_IOA_YX_485_WEATHER_ONLINE    0x001000UL /* 三合一气象设备在线。 */
#define IEC104_IOA_YX_485_TH_FAULT     0x001001UL /* 温湿度传感器故障。 */
#define IEC104_IOA_YX_485_WIND_FAULT   0x001011UL /* 风速风向传感器故障。 */
#define IEC104_IOA_YX_485_WATER_ONLINE 0x001020UL /* 水浸传感器在线。 */
#define IEC104_IOA_YX_485_WATER_ALARM  0x001021UL /* 水浸告警。 */
#define IEC104_IOA_YX_485_AC_ONLINE    0x001040UL /* 空调在线。 */
#define IEC104_IOA_YX_485_AC_FAULT     0x001041UL /* 空调故障或告警。 */
#define IEC104_IOA_YX_485_AC_COOLING   0x001042UL /* 空调制冷运行。 */
#define IEC104_IOA_YX_485_AC_HEATING   0x001043UL /* 空调制热运行。 */
#define IEC104_IOA_YX_485_AC_INNER_FAN 0x001044UL /* 空调内风机运行。 */
#define IEC104_IOA_YX_485_AC_OUTER_FAN 0x001045UL /* 空调外风机运行。 */
#define IEC104_IOA_YX_485_AC_HUMIDITY_HIGH 0x001046UL /* 空调湿度高告警。 */
#define IEC104_IOA_YX_485_AC_HIGH_TEMP 0x001047UL /* 空调高温告警。 */
#define IEC104_IOA_YX_485_AC_LOW_TEMP  0x001048UL /* 空调低温告警。 */
#define IEC104_IOA_YX_485_AC_CONDENSER_HIGH 0x001049UL /* 空调冷凝器高压告警。 */
#define IEC104_IOA_YX_485_AC_EVAPORATOR_FREEZE 0x00104AUL /* 空调蒸发器冻结告警。 */
#define IEC104_IOA_YX_485_AC_CABINET_SENSOR_FAULT 0x00104BUL /* 空调机柜温度传感器故障。 */
#define IEC104_IOA_YX_485_AC_EVAPORATOR_SENSOR_FAULT 0x00104CUL /* 空调蒸发器传感器故障。 */
#define IEC104_IOA_YX_485_AC_CONDENSER_SENSOR_FAULT 0x00104DUL /* 空调冷凝器传感器故障。 */
#define IEC104_IOA_YX_485_AC_HUMIDITY_SENSOR_FAULT 0x00104EUL /* 空调湿度传感器故障。 */

/* RS-485 子设备遥测 YC。 */
#define IEC104_IOA_YC_485_TH_TEMP        0x002000UL /* 温度，摄氏度。 */
#define IEC104_IOA_YC_485_TH_HUMIDITY    0x002001UL /* 湿度，%RH。 */
#define IEC104_IOA_YC_485_WIND_SPEED     0x002010UL /* 风速，m/s。 */
#define IEC104_IOA_YC_485_WIND_DIRECTION 0x002011UL /* 风向，度。 */
#define IEC104_IOA_YC_485_WATER_RAW      0x002020UL /* 水浸原始值。 */
#define IEC104_IOA_YC_485_WEATHER_RAIN   0x002031UL /* 三合一 513 雨量，mm；统计周期由设备定义。 */
#define IEC104_IOA_YC_485_AC_CABINET_TEMP 0x002040UL /* 空调机柜温度。 */
#define IEC104_IOA_YC_485_AC_EVAPORATOR_TEMP 0x002041UL /* 空调蒸发器温度。 */
#define IEC104_IOA_YC_485_AC_CONDENSER_TEMP 0x002042UL /* 空调冷凝器温度。 */
#define IEC104_IOA_YC_485_AC_HUMIDITY    0x002043UL /* 空调湿度。 */

/* RS-485 子设备遥调 YT。 */
#define IEC104_IOA_YT_485_AC_COOLING_START 0x003000UL /* 制冷启动温度阈值。 */
#define IEC104_IOA_YT_485_AC_COOLING_STOP 0x003001UL /* 制冷停止温度阈值。 */
#define IEC104_IOA_YT_485_AC_HEATING_START 0x003002UL /* 制热启动温度阈值。 */
#define IEC104_IOA_YT_485_AC_HEATING_STOP 0x003003UL /* 制热停止温度阈值。 */
#define IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM 0x003004UL /* 高温告警阈值。 */
#define IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM 0x003005UL /* 低温告警阈值。 */
#define IEC104_IOA_YT_485_AC_CONDENSER_HIGH_PROTECT 0x003006UL /* 冷凝器高压保护阈值。 */
#define IEC104_IOA_YT_485_AC_EVAPORATOR_FREEZE_PROTECT 0x003007UL /* 蒸发器防冻保护阈值。 */
#define IEC104_IOA_YT_485_AC_DEHUMIDIFY_START 0x003008UL /* 除湿启动湿度阈值。 */
#define IEC104_IOA_YT_485_AC_DEHUMIDIFY_STOP 0x003009UL /* 除湿停止湿度阈值。 */
#define IEC104_IOA_YT_485_AC_HUMIDITY_ALARM 0x00300AUL /* 湿度告警阈值。 */
#define IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION 0x00300BUL /* 湿度校准值。 */
#define IEC104_IOA_YT_485_AC_MODE_CONTROL 0x00300CUL /* 空调模式控制寄存器。 */

/* 一次采集保存所有业务模块数据，保证同一总召唤中的遥信和遥测来自同批快照。 */
typedef struct {
    motor_control_status_t motor;
    aux_output_status_t outputs;
    uint8_t ws2812_effect_active;
    weather_sensor_data_t weather;
    mt80_device_t mt80;
    xm8537_water_data_t water;
    ac_device_t ac;
} iec104_app_snapshot_t;

static uint16_t iec104_u16_read(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static void iec104_app_snapshot_read(iec104_app_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    motor_control_get_status(&snapshot->motor);
    aux_output_get_status(&snapshot->outputs);
    snapshot->ws2812_effect_active = ws2812_app_is_active();
    (void)modbus_weather_sensor_get(&snapshot->weather);
    (void)modbus_mt80_get(&snapshot->mt80);
    (void)modbus_xm8537_get(&snapshot->water);
    (void)modbus_ac_get(&snapshot->ac);
}

static uint8_t iec104_status_quality(uint8_t online, uint8_t sensor_fault)
{
    /* 离线数据标为无效且非当前值；传感器故障数据标为无效且被替代。 */
    if (online == 0U) {
        return IEC104_QUALITY_IV | IEC104_QUALITY_NT;
    }

    if (sensor_fault != 0U) {
        return IEC104_QUALITY_IV | IEC104_QUALITY_SB;
    }

    return 0U;
}

static uint8_t iec104_door_state_code(const motor_control_status_t *run_status)
{
    /* 工程约定：0 未知/中间，1 打开方向，2 关闭方向，3 故障。 */
    if (run_status->door_fault != MOTOR_FAULT_NONE) {
        return 3U;
    }
    if (run_status->door_action == MOTOR_DOOR_OPENING) return 1U;
    if (run_status->door_action == MOTOR_DOOR_CLOSING) return 2U;
    if (run_status->door_position == MOTOR_DOOR_OPENED) return 1U;
    if (run_status->door_position == MOTOR_DOOR_CLOSED) return 2U;

    return 0U;
}

static uint8_t iec104_platform_state_code(const motor_control_status_t *run_status)
{
    /* 工程约定：0 未知/中间，1 伸出方向，2 收回方向，3 故障。 */
    if (run_status->platform_fault != MOTOR_FAULT_NONE) {
        return 3U;
    }
    if (run_status->platform_action == MOTOR_PLATFORM_EXTENDING) return 1U;
    if (run_status->platform_action == MOTOR_PLATFORM_RETRACTING) return 2U;
    if (run_status->platform_position == MOTOR_PLATFORM_EXTENDED) return 1U;
    if (run_status->platform_position == MOTOR_PLATFORM_RETRACTED) return 2U;

    return 0U;
}

static uint8_t iec104_ac_has_fault(const ac_device_t *ac)
{
    const ac_status_t *status = &ac->run_status;

    return ac->status.sensor_fault || status->humidity_high_alarm || status->condenser_high_pressure_alarm ||
        status->evaporator_freeze_alarm || status->cooling_alarm || status->high_temp_alarm || status->low_temp_alarm;
}

static const struct {
    uint16_t reg;
    uint16_t mask;
} mt80_status_bits[] = {
    { MT80_REG_INVERTER_STATE, 0x0FFFU },
    { MT80_REG_CHARGE1_STATE, 0x3FD1U },
    { MT80_REG_LOAD_STATE, 0x0FF3U },
    { MT80_REG_BATTERY_STATE, 0x8700U },
};

static void iec104_app_fill_status_points(const iec104_app_snapshot_t *snapshot, iec104_status_point_t *points)
{
    const motor_control_status_t *run_status = &snapshot->motor;
    const aux_output_status_t *outputs = &snapshot->outputs;
    const weather_sensor_data_t *weather = &snapshot->weather;
    const xm8537_water_data_t *water = &snapshot->water;
    const ac_device_t *ac = &snapshot->ac;
    uint8_t weather_quality = iec104_status_quality(weather->status.online && weather->measurement_valid, 0U);
    uint8_t water_quality = iec104_status_quality(water->status.online, 0U);
    uint8_t ac_quality = iec104_status_quality(ac->status.online, 0U);
    uint8_t door_running = run_status->door_action != MOTOR_DOOR_STOPPED;
    uint8_t platform_running = run_status->platform_action != MOTOR_PLATFORM_STOPPED;

    points[0] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_OPENED, (run_status->door_position == MOTOR_DOOR_OPENED && !door_running && !run_status->door_fault), 0U };
    points[1] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_CLOSED, (run_status->door_position == MOTOR_DOOR_CLOSED && !door_running && !run_status->door_fault), 0U };
    points[2] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_EXTENDED, (run_status->platform_position == MOTOR_PLATFORM_EXTENDED && !platform_running && !run_status->platform_fault), 0U };
    points[3] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_RETRACTED, (run_status->platform_position == MOTOR_PLATFORM_RETRACTED && !platform_running && !run_status->platform_fault), 0U };
    points[4] = (iec104_status_point_t){ IEC104_IOA_YX_EM_STOP, key_is_pressed_now(KEY_ID_EM_STOP), 0U };
    points[5] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_RUNNING, door_running, 0U };
    points[6] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_RUNNING, platform_running, 0U };
    points[7] = (iec104_status_point_t){ IEC104_IOA_YX_WS2812_EFFECT_ACTIVE, snapshot->ws2812_effect_active, 0U };
    points[8] = (iec104_status_point_t){ IEC104_IOA_YX_INDICATOR_LIGHT_ON, outputs->indicator_enabled, 0U };
    points[9] = (iec104_status_point_t){ IEC104_IOA_YX_INTERIOR_LIGHT_ON, outputs->interior_light_enabled, 0U };
    points[10] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_HEATER_ENABLED, outputs->door_heater_enabled, 0U };

    points[11] = (iec104_status_point_t){ IEC104_IOA_YX_485_WEATHER_ONLINE, weather->status.online && weather->measurement_valid, weather_quality };
    points[12] = (iec104_status_point_t){ IEC104_IOA_YX_485_TH_FAULT, weather->th_fault, weather_quality };

    points[13] = (iec104_status_point_t){ IEC104_IOA_YX_485_WIND_FAULT, weather->wind_fault, weather_quality };

    points[14] = (iec104_status_point_t){ IEC104_IOA_YX_485_WATER_ONLINE, water->status.online, water_quality };
    points[15] = (iec104_status_point_t){ IEC104_IOA_YX_485_WATER_ALARM, water->water_detected,
        iec104_status_quality(water->status.online, water->status.sensor_fault) };


    points[16] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_ONLINE, ac->status.online, ac_quality };
    points[17] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_FAULT, iec104_ac_has_fault(ac), ac_quality };
    points[18] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_COOLING, ac->run_status.cooling_on, ac_quality };
    points[19] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HEATING, ac->run_status.heater_on, ac_quality };
    points[20] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_INNER_FAN, ac->run_status.inner_fan_on, ac_quality };
    points[21] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_OUTER_FAN, ac->run_status.outer_fan_on, ac_quality };
    points[22] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HUMIDITY_HIGH, ac->run_status.humidity_high_alarm, ac_quality };
    points[23] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HIGH_TEMP, ac->run_status.high_temp_alarm, ac_quality };
    points[24] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_LOW_TEMP, ac->run_status.low_temp_alarm, ac_quality };
    points[25] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CONDENSER_HIGH, ac->run_status.condenser_high_pressure_alarm, ac_quality };
    points[26] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_EVAPORATOR_FREEZE, ac->run_status.evaporator_freeze_alarm, ac_quality };
    points[27] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CABINET_SENSOR_FAULT, ac->run_status.cabinet_sensor_fault, ac_quality };
    points[28] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_EVAPORATOR_SENSOR_FAULT, ac->run_status.evaporator_sensor_fault, ac_quality };
    points[29] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CONDENSER_SENSOR_FAULT, ac->run_status.condenser_sensor_fault, ac_quality };
    points[30] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HUMIDITY_SENSOR_FAULT, ac->run_status.humidity_sensor_fault, ac_quality };
    points[31] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_LIMIT_FAULT, ((run_status->door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0), 0U };
    points[32] = (iec104_status_point_t){ IEC104_IOA_YX_PLATFORM_LIMIT_FAULT, ((run_status->platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0), 0U };
    const mt80_device_t *mt80 = &snapshot->mt80;
    const uint8_t coil_values[MT80_COIL_COUNT] = { mt80->clear_energy, mt80->fault_reset,
        mt80->inverter_output, mt80->remote_control, mt80->dc_output };
    uint8_t point = 39U;
    for (uint8_t i = 0U; i < MT80_COIL_COUNT; i++) {
        points[33U + i] = (iec104_status_point_t){ 0x011000UL + mt80_coils[i], coil_values[i],
            iec104_status_quality(mt80->status.online && (mt80->coil_valid & (1U << i)), 0U) };
    }
    points[38] = (iec104_status_point_t){ 0x01100FUL, mt80->status.online, 0U };
    for (uint8_t group = 0U; group < sizeof(mt80_status_bits) / sizeof(mt80_status_bits[0]); group++) {
        for (uint8_t index = 0U; index < MT80_REGISTER_COUNT; index++) {
            if (mt80_registers[index].address != mt80_status_bits[group].reg) continue;
            uint16_t raw;
            memcpy(&raw, (const uint8_t *)mt80 + mt80_registers[index].offset, sizeof(raw));
            for (uint8_t bit = 0U; bit < 16U; bit++) {
                if (!(mt80_status_bits[group].mask & (1U << bit))) continue;
                points[point++] = (iec104_status_point_t){
                    0x040000UL + (mt80_status_bits[group].reg - 0x3000UL) * 16U + bit,
                    (uint8_t)((raw >> bit) & 1U),
                    iec104_status_quality(mt80->status.online && mt80->register_valid[index], 0U) };
            }
            break;
        }
    }
}

static void iec104_app_fill_measure_points(const iec104_app_snapshot_t *snapshot, iec104_measure_point_t *points)
{
    const weather_sensor_data_t *weather = &snapshot->weather;
    const xm8537_water_data_t *water = &snapshot->water;
    const ac_device_t *ac = &snapshot->ac;
    uint8_t th_quality = iec104_status_quality(weather->status.online && weather->measurement_valid, weather->th_fault);
    uint8_t wind_quality = iec104_status_quality(weather->status.online && weather->measurement_valid, weather->wind_fault);
    uint8_t water_quality = iec104_status_quality(water->status.online, water->status.sensor_fault);
    uint8_t rain_quality = iec104_status_quality(weather->status.online && weather->measurement_valid, 0U);
    uint8_t ac_cabinet_quality = iec104_status_quality(ac->status.online, ac->run_status.cabinet_sensor_fault);
    uint8_t ac_evaporator_quality = iec104_status_quality(ac->status.online, ac->run_status.evaporator_sensor_fault);
    uint8_t ac_condenser_quality = iec104_status_quality(ac->status.online, ac->run_status.condenser_sensor_fault);
    uint8_t ac_humidity_quality = iec104_status_quality(ac->status.online, ac->run_status.humidity_sensor_fault);
    uint8_t ac_param_quality = iec104_status_quality(ac->status.online, 0U);
    uint8_t ac_param_invalid = IEC104_QUALITY_IV | IEC104_QUALITY_NT;

    points[0] = (iec104_measure_point_t){ IEC104_IOA_YC_DOOR_STATE,
        (float)iec104_door_state_code(&snapshot->motor), 0U };
    points[1] = (iec104_measure_point_t){ IEC104_IOA_YC_HELIPED_STATE,
        (float)iec104_platform_state_code(&snapshot->motor), 0U };
    points[2] = (iec104_measure_point_t){ IEC104_IOA_YC_485_TH_TEMP,
        (float)weather->temperature_deci_c / 10.0F, th_quality };
    points[3] = (iec104_measure_point_t){ IEC104_IOA_YC_485_TH_HUMIDITY,
        (float)weather->humidity_deci_rh / 10.0F, th_quality };
    points[4] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WIND_SPEED,
        (float)weather->speed_deci_ms / 10.0F, wind_quality };
    points[5] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WIND_DIRECTION,
        (float)weather->direction_deg, wind_quality };
    points[6] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WATER_RAW,
        (float)water->raw, water_quality };
    points[7] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WEATHER_RAIN,
        (float)weather->rain_deci_mm / 10.0F, rain_quality };
    points[8] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_CABINET_TEMP,
        (float)ac->params.cabinet_temp_deci_c / 10.0F, ac_cabinet_quality };
    points[9] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_EVAPORATOR_TEMP,
        (float)ac->params.evaporator_temp_deci_c / 10.0F, ac_evaporator_quality };
    points[10] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_CONDENSER_TEMP,
        (float)ac->params.condenser_temp_deci_c / 10.0F, ac_condenser_quality };
    points[11] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_HUMIDITY,
        (float)ac->params.humidity_deci_rh / 10.0F, ac_humidity_quality };
    points[12] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_COOLING_START,
        (float)ac->params.cooling_start_deci_c / 10.0F,
        ac->params.marks.bit.cooling_start ? ac_param_quality : ac_param_invalid };
    points[13] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_COOLING_STOP,
        (float)ac->params.cooling_stop_deci_c / 10.0F,
        ac->params.marks.bit.cooling_stop ? ac_param_quality : ac_param_invalid };
    points[14] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_HEATING_START,
        (float)ac->params.heating_start_deci_c / 10.0F,
        ac->params.marks.bit.heating_start ? ac_param_quality : ac_param_invalid };
    points[15] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_HEATING_STOP,
        (float)ac->params.heating_stop_deci_c / 10.0F,
        ac->params.marks.bit.heating_stop ? ac_param_quality : ac_param_invalid };
    points[16] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM,
        (float)ac->params.high_temp_alarm_deci_c / 10.0F,
        ac->params.marks.bit.high_temp_alarm ? ac_param_quality : ac_param_invalid };
    points[17] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM,
        (float)ac->params.low_temp_alarm_deci_c / 10.0F,
        ac->params.marks.bit.low_temp_alarm ? ac_param_quality : ac_param_invalid };
    points[18] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_CONDENSER_HIGH_PROTECT,
        (float)ac->params.condenser_high_protect_deci_c / 10.0F,
        ac->params.marks.bit.condenser_high_protect ? ac_param_quality : ac_param_invalid };
    points[19] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_EVAPORATOR_FREEZE_PROTECT,
        (float)ac->params.evaporator_freeze_protect_deci_c / 10.0F,
        ac->params.marks.bit.evaporator_freeze_protect ? ac_param_quality : ac_param_invalid };
    points[20] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_DEHUMIDIFY_START,
        (float)ac->params.dehumidify_start_deci_rh / 10.0F,
        ac->params.marks.bit.dehumidify_start ? ac_param_quality : ac_param_invalid };
    points[21] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_DEHUMIDIFY_STOP,
        (float)ac->params.dehumidify_stop_deci_rh / 10.0F,
        ac->params.marks.bit.dehumidify_stop ? ac_param_quality : ac_param_invalid };
    points[22] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_HUMIDITY_ALARM,
        (float)ac->params.humidity_alarm_deci_rh / 10.0F,
        ac->params.marks.bit.humidity_alarm ? ac_param_quality : ac_param_invalid };
    points[23] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION,
        (float)ac->params.humidity_calibration,
        ac->params.marks.bit.humidity_calibration ? ac_param_quality : ac_param_invalid };
    points[24] = (iec104_measure_point_t){ IEC104_IOA_YT_485_AC_MODE_CONTROL,
        (float)ac->params.mode_control,
        ac->params.marks.bit.mode_control ? ac_param_quality : ac_param_invalid };
    points[25] = (iec104_measure_point_t){ 0x002012UL, (float)weather->wind_force, wind_quality };
    points[26] = (iec104_measure_point_t){ 0x002013UL, (float)weather->direction_sector, wind_quality };
    for (uint8_t i = 0U; i < MT80_REGISTER_COUNT; i++) {
        const mt80_register_t *point = &mt80_registers[i];
        uint32_t raw = 0U;
        memcpy(&raw, (const uint8_t *)&snapshot->mt80 + point->offset, point->words == 1U ? sizeof(uint16_t) : sizeof(uint32_t));
        float value = (float)raw;
        if (point->is_signed) {
            if (point->words == 2U && (raw & 0x80000000UL)) value = -(float)(~raw + 1U);
            if (point->words == 1U && (raw & 0x8000U)) value = (float)((int32_t)raw - 65536);
        }
        points[27U + i] = (iec104_measure_point_t){ 0x010000UL + point->address,
            value / point->divisor,
            iec104_status_quality(snapshot->mt80.status.online && snapshot->mt80.register_valid[i], 0U) };
    }

    /* 参数区尚未收到有效寄存器数据时，四个空调遥测点统一标为无效。 */
    if (ac->params.marks.raw == 0U) {
        points[8].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[9].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[10].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[11].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
    }
}

void iec104_app_collect_status_points(iec104_status_point_t *points)
{
    iec104_app_snapshot_t snapshot;

    iec104_app_snapshot_read(&snapshot);
    iec104_app_fill_status_points(&snapshot, points);
}

void iec104_app_collect_interrogation(iec104_status_point_t *status_points, iec104_measure_point_t *measure_points)
{
    iec104_app_snapshot_t snapshot;

    iec104_app_snapshot_read(&snapshot);
    iec104_app_fill_status_points(&snapshot, status_points);
    iec104_app_fill_measure_points(&snapshot, measure_points);
}

/*
 * 遥控点描述表：IOA → 执行动作。
 * is_known 与 execute 均由查表推导，新增遥控点只需在此追加一行。
 */
typedef struct {
    uint32_t ioa;
    void (*execute)(void);
} iec104_command_desc_t;

static void iec104_app_ac_power_set(uint8_t on)
{
    ac_device_t ac;

    memset(&ac, 0, sizeof(ac));
    if (modbus_ac_get(&ac)) {
        (void)modbus_ac_queue_write_status(ac.slave_addr, AC_STATUS_POWER_MONITOR, on != 0U);
    }
}

static void iec104_app_ac_power_on(void)
{
    iec104_app_ac_power_set(1U);
}

static void iec104_app_ac_power_off(void)
{
    iec104_app_ac_power_set(0U);
}

static const iec104_command_desc_t iec104_command_table[] = {
    { IEC104_IOA_YK_DOOR_OPEN,       motor_control_request_door_open },
    { IEC104_IOA_YK_DOOR_CLOSE,      motor_control_request_door_close },
    { IEC104_IOA_YK_HELIPED_EXTEND,  motor_control_request_platform_extend },
    { IEC104_IOA_YK_HELIPED_RETRACT, motor_control_request_platform_retract },
    { IEC104_IOA_YK_485_AC_POWER_ON,  iec104_app_ac_power_on },
    { IEC104_IOA_YK_485_AC_POWER_OFF, iec104_app_ac_power_off },
    { IEC104_IOA_YK_INTERIOR_LIGHT_ON,   aux_output_enable_interior_light },
    { IEC104_IOA_YK_INTERIOR_LIGHT_OFF,  aux_output_disable_interior_light },
    { IEC104_IOA_YK_DOOR_HEATER_ENABLE,  aux_output_enable_door_heater },
    { IEC104_IOA_YK_DOOR_HEATER_DISABLE, aux_output_disable_door_heater },
};

#define IEC104_COMMAND_TABLE_COUNT \
    (sizeof(iec104_command_table) / sizeof(iec104_command_table[0]))

static const iec104_command_desc_t *iec104_command_find(uint32_t ioa)
{
    uint8_t i;

    for (i = 0U; i < IEC104_COMMAND_TABLE_COUNT; i++) {
        if (iec104_command_table[i].ioa == ioa) {
            return &iec104_command_table[i];
        }
    }

    return NULL;
}

uint8_t iec104_app_remote_control_is_known(uint32_t ioa)
{
    if (iec104_command_find(ioa) != NULL || (ioa >= 0x230U && ioa <= 0x238U) || ioa == 0x240U || ioa == 0x241U) return 1U;
    for (uint8_t i = 0U; i < MT80_COIL_COUNT; i++) {
        uint32_t on_ioa = 0x300UL + 2UL * mt80_coils[i];
        if (ioa == on_ioa || ioa == on_ioa + 1U) return 1U;
    }
    return 0U;
}

uint8_t iec104_app_remote_control_allowed(uint32_t ioa)
{
    if (ioa == IEC104_IOA_YK_DOOR_OPEN || ioa == IEC104_IOA_YK_DOOR_CLOSE ||
        ioa == IEC104_IOA_YK_HELIPED_EXTEND || ioa == IEC104_IOA_YK_HELIPED_RETRACT) {
        motor_control_status_t status;
        motor_control_get_status(&status);
        return !status.key_control_active;
    }
    return 1U;
}

void iec104_app_remote_control_execute(uint32_t ioa)
{
    const iec104_command_desc_t *command = iec104_command_find(ioa);
    if (!iec104_app_remote_control_allowed(ioa)) return;
    if (ioa >= 0x230U && ioa <= 0x238U) {
        static const modbus_dev_type_t devices[] = { MODBUS_DEV_MACHINE_EXTEND, MODBUS_DEV_POLE_RAISE, MODBUS_DEV_SOLAR_UNFOLD };
        static const wanrong_command_e commands[] = { WANRONG_FORWARD, WANRONG_REVERSE, WANRONG_STOP };
        (void)modbus_wanrong_queue_command(devices[(ioa - 0x230U) / 3U], commands[(ioa - 0x230U) % 3U]);
        return;
    }
    if (ioa == 0x240U || ioa == 0x241U) {
        (void)modbus_weather_sensor_queue_write(ioa == 0x240U ? WEATHER_SENSOR_REG_SPEED_ZERO : WEATHER_SENSOR_REG_RAIN_ZERO,
            ioa == 0x240U ? 0xAAU : 0x5AU);
        return;
    }
    for (uint8_t i = 0U; i < MT80_COIL_COUNT; i++) {
        uint32_t on_ioa = 0x300UL + 2UL * mt80_coils[i];
        if (ioa == on_ioa || ioa == on_ioa + 1U) {
            (void)modbus_mt80_queue_write_coil(mt80_coils[i], ioa == on_ioa);
            return;
        }
    }

    if ((command != NULL) && (command->execute != NULL)) {
        command->execute();
    }
}

/*
 * 遥调点描述表：IOA → 目标寄存器 + 信息体值长度。
 * index/value_length/register 三处映射统一由本表推导，表内下标即为遥调点序号，
 * 数组长度须与 IEC104_APP_TUNING_POINT_COUNT 一致。
 */
typedef struct {
    uint32_t ioa;
    uint16_t reg;
    uint8_t value_length;
    uint8_t target;
} iec104_tuning_desc_t;



static const iec104_tuning_desc_t iec104_tuning_table[] = {
    { IEC104_IOA_YT_485_AC_COOLING_START,           AC_PARAM_REG_COOLING_START,          4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_COOLING_STOP,            AC_PARAM_REG_COOLING_STOP,           4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_HEATING_START,           AC_PARAM_REG_HEATING_START,          4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_HEATING_STOP,            AC_PARAM_REG_HEATING_STOP,           4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM,         AC_PARAM_REG_HIGH_TEMP_ALARM,        4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM,          AC_PARAM_REG_LOW_TEMP_ALARM,         4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_CONDENSER_HIGH_PROTECT,  AC_PARAM_REG_CONDENSER_HIGH_PROTECT, 4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_EVAPORATOR_FREEZE_PROTECT, AC_PARAM_REG_EVAPORATOR_FREEZE,    4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_DEHUMIDIFY_START,        AC_PARAM_REG_DEHUMIDIFY_START,       4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_DEHUMIDIFY_STOP,         AC_PARAM_REG_DEHUMIDIFY_STOP,        4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_HUMIDITY_ALARM,          AC_PARAM_REG_HUMIDITY_ALARM,         4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION,    AC_PARAM_REG_HUMIDITY_CALIBRATION,   4U, IEC104_TUNING_TARGET_AC },
    { IEC104_IOA_YT_485_AC_MODE_CONTROL,            AC_PARAM_REG_MODE_CONTROL,           2U, IEC104_TUNING_TARGET_AC },
    { 0x003011UL, WEATHER_SENSOR_REG_DIRECTION_OFFSET, 2U, IEC104_TUNING_TARGET_WEATHER },
    { 0x003012UL, WEATHER_SENSOR_REG_RAIN_SENSITIVITY, 2U, IEC104_TUNING_TARGET_WEATHER },
    { 0x01960FUL, MT80_REG_BATTERY_TYPE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019610UL, MT80_REG_BATTERY_CAPACITY, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019611UL, MT80_REG_TEMPERATURE_COMPENSATION, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019613UL, MT80_REG_EQUALIZE_MINUTES, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019614UL, MT80_REG_BOOST_MINUTES, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019615UL, MT80_REG_OVERVOLTAGE_DISCONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019616UL, MT80_REG_CHARGE_VOLTAGE_LIMIT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019617UL, MT80_REG_OVERVOLTAGE_RECONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019618UL, MT80_REG_EQUALIZE_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019619UL, MT80_REG_BOOST_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961AUL, MT80_REG_FLOAT_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961BUL, MT80_REG_BOOST_RECONNECT_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961CUL, MT80_REG_LOW_VOLTAGE_RECONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961DUL, MT80_REG_UNDERVOLTAGE_ALARM_RECONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961EUL, MT80_REG_UNDERVOLTAGE_ALARM, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01961FUL, MT80_REG_LOW_VOLTAGE_DISCONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019620UL, MT80_REG_DISCHARGE_VOLTAGE_LIMIT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019621UL, MT80_REG_LITHIUM_PROTECTION, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019622UL, MT80_REG_LOW_CHARGE_TEMPERATURE_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019623UL, MT80_REG_LOW_DISCHARGE_TEMPERATURE_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019624UL, MT80_REG_RTC_MINUTE_SECOND, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019631UL, MT80_REG_INVERTER_VOLTAGE_SETTING, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019632UL, MT80_REG_INVERTER_FREQUENCY_SETTING, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019633UL, MT80_REG_INVERTER_LOW_VOLTAGE_DISCONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019634UL, MT80_REG_INVERTER_LOW_VOLTAGE_RECONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019635UL, MT80_REG_INVERTER_OVERVOLTAGE_RECONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019636UL, MT80_REG_INVERTER_OVERVOLTAGE_DISCONNECT, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019656UL, MT80_REG_BATTERY_HIGH_TEMPERATURE_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019657UL, MT80_REG_BATTERY_LOW_TEMPERATURE_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019658UL, MT80_REG_DEVICE_HIGH_TEMPERATURE_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019659UL, MT80_REG_DEVICE_HIGH_TEMPERATURE_RECOVER_RAW, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01965CUL, MT80_REG_LIGHT_ON_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01965DUL, MT80_REG_LIGHT_ON_DELAY, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01965EUL, MT80_REG_LIGHT_OFF_VOLTAGE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x01965FUL, MT80_REG_LIGHT_OFF_DELAY, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019660UL, MT80_REG_LOAD_MODE, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019661UL, MT80_REG_WORK_DURATION1, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019662UL, MT80_REG_WORK_DURATION2, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019663UL, MT80_REG_LOAD_ON_TIME1, 4U, IEC104_TUNING_TARGET_MT80 },
    { 0x019666UL, MT80_REG_LOAD_OFF_TIME1, 4U, IEC104_TUNING_TARGET_MT80 },
    { 0x019669UL, MT80_REG_LOAD_ON_TIME2, 4U, IEC104_TUNING_TARGET_MT80 },
    { 0x01966CUL, MT80_REG_LOAD_OFF_TIME2, 4U, IEC104_TUNING_TARGET_MT80 },
    { 0x01966FUL, MT80_REG_NIGHT_DURATION, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019670UL, MT80_REG_SELECTED_PERIOD, 2U, IEC104_TUNING_TARGET_MT80 },
    { 0x019671UL, MT80_REG_MANUAL_DEFAULT_SWITCH, 2U, IEC104_TUNING_TARGET_MT80 },

};

#define IEC104_TUNING_TABLE_COUNT \
    (sizeof(iec104_tuning_table) / sizeof(iec104_tuning_table[0]))

/* 表长须与点数宏一致，否则遥调数组越界。编译期校验，长度不符则数组维度为负报错。 */
typedef char iec104_tuning_table_size_check[
    (IEC104_TUNING_TABLE_COUNT == IEC104_APP_TUNING_POINT_COUNT) ? 1 : -1];

int8_t iec104_app_tuning_index(uint32_t ioa)
{
    uint8_t i;

    for (i = 0U; i < IEC104_TUNING_TABLE_COUNT; i++) {
        if (iec104_tuning_table[i].ioa == ioa) {
            return (int8_t)i;
        }
    }

    return -1;
}

uint8_t iec104_app_tuning_value_length(uint32_t ioa)
{
    int8_t index = iec104_app_tuning_index(ioa);

    return (index < 0) ? 0U : iec104_tuning_table[index].value_length;
}

static uint16_t iec104_app_tuning_register(uint32_t ioa)
{
    int8_t index = iec104_app_tuning_index(ioa);

    return (index < 0) ? 0U : iec104_tuning_table[index].reg;
}

uint8_t iec104_app_tuning_target(uint32_t ioa)
{
    int8_t index = iec104_app_tuning_index(ioa);

    return (index < 0) ? 0xFFU : iec104_tuning_table[index].target;
}

static uint8_t iec104_app_tuning_value_is_valid(uint32_t ioa, int16_t value)
{
    switch (ioa) {
    case IEC104_IOA_YT_485_AC_COOLING_START:
        return (value >= 200) && (value <= 550);

    case IEC104_IOA_YT_485_AC_COOLING_STOP:
        return (value >= 150) && (value <= 350);

    case IEC104_IOA_YT_485_AC_HEATING_START:
        return (value >= -300) && (value <= 150);

    case IEC104_IOA_YT_485_AC_HEATING_STOP:
        return (value >= 0) && (value <= 230);

    case IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM:
        return (value >= 300) && (value <= 600);

    case IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM:
        return (value >= -100) && (value <= 100);

    default:
        return 1U; /* 协议未给出其余参数的数值范围。 */
    }
}

uint8_t iec104_app_tuning_value_parse(uint32_t ioa, const uint8_t *data, uint32_t *value, uint8_t *value_length, uint32_t *register_value)
{
    float float_value;
    int16_t integer_value;

    uint8_t target = iec104_app_tuning_target(ioa);
    if (target == 0xFFU) return 0U;
    if (target != IEC104_TUNING_TARGET_AC) {
        uint8_t frame[MT80_WRITE_FRAME_MAX];
        uint16_t regs[3];
        *value_length = iec104_app_tuning_value_length(ioa);
        if (*value_length == 4U) {
            *value = (uint32_t)data[0] | ((uint32_t)data[1] << 8U) | ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
            if (*value > 86399UL) return 0U;
            *register_value = *value;
            regs[0] = *value % 60U;
            regs[1] = (*value / 60U) % 60U;
            regs[2] = *value / 3600U;
        } else {
            *value = iec104_u16_read(data);
            *register_value = *value;
            regs[0] = (uint16_t)*value;
        }
        if (target == IEC104_TUNING_TARGET_MT80) {
            return modbus_mt80_build_write_params(MODULE_MODBUS_MT80_ADDR, iec104_app_tuning_register(ioa),
                regs, *value_length == 4U ? 3U : 1U, frame, sizeof(frame)) != 0U;
        }
        return modbus_weather_sensor_build_write(MODULE_MODBUS_WEATHER_SENSOR_ADDR, iec104_app_tuning_register(ioa),
            regs[0], frame, sizeof(frame)) != 0U;
    }
    /* 空调模式使用整数，其余空调参数使用工程值浮点数。 */
    if (ioa == IEC104_IOA_YT_485_AC_MODE_CONTROL) {
        *value = iec104_u16_read(data);
        *value_length = 2U;
        *register_value = (uint16_t)*value;
        return (*register_value <= 3U);
    }

    memcpy(&float_value, data, sizeof(float_value));
    if ((float_value != float_value) || (float_value < -3276.8F) || (float_value > 3276.7F)) {
        return 0U;
    }

    memcpy(value, data, sizeof(float_value));
    *value_length = 4U;
    if (ioa == IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION) {
        integer_value = (int16_t)float_value;
        if ((float)integer_value != float_value) {
            return 0U;
        }
        *register_value = (uint16_t)integer_value;
        return 1U;
    }

    /* 空调寄存器以 0.1 为单位保存有符号值。 */
    integer_value = (int16_t)(float_value * 10.0F);
    if (!iec104_app_tuning_value_is_valid(ioa, integer_value)) {
        return 0U;
    }
    *register_value = (uint16_t)integer_value;
    return 1U;
}

uint8_t iec104_app_tuning_write(uint32_t ioa, uint32_t value)
{
    uint16_t reg = iec104_app_tuning_register(ioa);
    ac_device_t ac;
    uint16_t regs[3];
    switch (iec104_app_tuning_target(ioa)) {
    case IEC104_TUNING_TARGET_AC:
        return modbus_ac_get(&ac) && modbus_ac_queue_write_param(ac.slave_addr, reg, (int16_t)value);
    case IEC104_TUNING_TARGET_WEATHER:
        return modbus_weather_sensor_queue_write(reg, value);
    case IEC104_TUNING_TARGET_MT80:
        if (iec104_app_tuning_value_length(ioa) == 4U) {
            if (value > 86399UL) return 0U;
            regs[0] = value % 60U;
            regs[1] = (value / 60U) % 60U;
            regs[2] = value / 3600U;
            return modbus_mt80_queue_write_params(reg, regs, 3U);
        }
        if (value > UINT16_MAX) return 0U;
        regs[0] = (uint16_t)value;
        return modbus_mt80_queue_write_params(reg, regs, 1U);
    default:
        return 0U;
    }
}

uint8_t iec104_app_tuning_write_group(const uint32_t *ioas, const uint32_t *values, uint8_t count)
{
    uint8_t start = 0U;
    while (start < count) {
        uint8_t length = 1U;
        if (iec104_app_tuning_target(ioas[start]) == IEC104_TUNING_TARGET_MT80 && iec104_app_tuning_value_length(ioas[start]) == 2U) {
            uint16_t regs[MODULE_MODBUS_MT80_MAX_REGS];
            while (start + length < count && length < MODULE_MODBUS_MT80_MAX_REGS &&
                iec104_app_tuning_value_length(ioas[start + length]) == 2U && ioas[start + length] == ioas[start] + length) length++;
            for (uint8_t i = 0U; i < length; i++) {
                if (values[start + i] > UINT16_MAX) return 0U;
                regs[i] = (uint16_t)values[start + i];
            }
            if (!modbus_mt80_queue_write_params(iec104_app_tuning_register(ioas[start]), regs, length)) return 0U;
        } else if (!iec104_app_tuning_write(ioas[start], values[start])) {
            return 0U;
        }
        start += length;
    }
    return 1U;
}
