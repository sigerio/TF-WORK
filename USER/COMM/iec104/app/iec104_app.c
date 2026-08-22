#include "iec104_app.h"

#include <string.h>

#include "../USER/CTRL/KEY/key.h"
#include "../../USER/CTRL/LIGHT/light_output.h"
#include "../../USER/CTRL/MOTO/motor_driver.h"
#include "../../USER/CTRL/MOTO/motor_business.h"
#include "../../USER/CTRL/MOTO/motor_motion.h"
#include "../../USER/CTRL/module_ws2812/module_ws2812_status.h"
#include "../../modbus/tf_modbus_ac.h"
#include "../../modbus/tf_modbus_xm7828.h"
#include "../../modbus/tf_modbus_xm8189.h"
#include "../../modbus/tf_modbus_xm8537.h"
#include "../../modbus/tf_modbus_xm9505.h"

#define IEC104_QUALITY_IV 0x80U
#define IEC104_QUALITY_NT 0x40U
#define IEC104_QUALITY_SB 0x20U

/* 工程点表：0x000xxx 门机，0x001xxx 遥信，0x002xxx 遥测，0x003xxx 遥调。 */
#define IEC104_IOA_YX_DOOR_OPENED       0x000001UL
#define IEC104_IOA_YX_DOOR_CLOSED       0x000002UL
#define IEC104_IOA_YX_HELIPED_EXTENDED  0x000003UL
#define IEC104_IOA_YX_HELIPED_RETRACTED 0x000004UL
#define IEC104_IOA_YX_EM_STOP           0x000010UL
#define IEC104_IOA_YX_DOOR_RUNNING      0x000011UL
#define IEC104_IOA_YX_HELIPED_RUNNING   0x000012UL
#define IEC104_IOA_YX_WS2812_EFFECT_ACTIVE 0x000013UL
#define IEC104_IOA_YX_INDICATOR_LIGHT_ON   0x000014UL
#define IEC104_IOA_YX_INTERIOR_LIGHT_ON    0x000015UL
#define IEC104_IOA_YX_DECORATIVE_LIGHT_ON  0x000016UL

#define IEC104_IOA_YC_DOOR_STATE    0x000100UL
#define IEC104_IOA_YC_HELIPED_STATE 0x000101UL

#define IEC104_IOA_YK_DOOR_OPEN       0x000200UL
#define IEC104_IOA_YK_DOOR_CLOSE      0x000201UL
#define IEC104_IOA_YK_HELIPED_EXTEND  0x000210UL
#define IEC104_IOA_YK_HELIPED_RETRACT 0x000211UL
#define IEC104_IOA_YK_INDICATOR_LIGHT_ON  0x000220UL
#define IEC104_IOA_YK_INDICATOR_LIGHT_OFF 0x000221UL
#define IEC104_IOA_YK_INTERIOR_LIGHT_ON   0x000222UL
#define IEC104_IOA_YK_INTERIOR_LIGHT_OFF  0x000223UL
#define IEC104_IOA_YK_DECORATIVE_LIGHT_ON 0x000224UL
#define IEC104_IOA_YK_DECORATIVE_LIGHT_OFF 0x000225UL

#define IEC104_IOA_YX_485_TH_ONLINE    0x001000UL
#define IEC104_IOA_YX_485_TH_FAULT     0x001001UL
#define IEC104_IOA_YX_485_WIND_ONLINE  0x001010UL
#define IEC104_IOA_YX_485_WIND_FAULT   0x001011UL
#define IEC104_IOA_YX_485_WATER_ONLINE 0x001020UL
#define IEC104_IOA_YX_485_WATER_ALARM  0x001021UL
#define IEC104_IOA_YX_485_RAIN_ONLINE  0x001030UL
#define IEC104_IOA_YX_485_RAIN_FAULT   0x001031UL
#define IEC104_IOA_YX_485_AC_ONLINE    0x001040UL
#define IEC104_IOA_YX_485_AC_FAULT     0x001041UL
#define IEC104_IOA_YX_485_AC_COOLING   0x001042UL
#define IEC104_IOA_YX_485_AC_HEATING   0x001043UL
#define IEC104_IOA_YX_485_AC_INNER_FAN 0x001044UL
#define IEC104_IOA_YX_485_AC_OUTER_FAN 0x001045UL
#define IEC104_IOA_YX_485_AC_HUMIDITY_HIGH 0x001046UL
#define IEC104_IOA_YX_485_AC_HIGH_TEMP 0x001047UL
#define IEC104_IOA_YX_485_AC_LOW_TEMP  0x001048UL
#define IEC104_IOA_YX_485_AC_CONDENSER_HIGH 0x001049UL
#define IEC104_IOA_YX_485_AC_EVAPORATOR_FREEZE 0x00104AUL
#define IEC104_IOA_YX_485_AC_CABINET_SENSOR_FAULT 0x00104BUL
#define IEC104_IOA_YX_485_AC_EVAPORATOR_SENSOR_FAULT 0x00104CUL
#define IEC104_IOA_YX_485_AC_CONDENSER_SENSOR_FAULT 0x00104DUL
#define IEC104_IOA_YX_485_AC_HUMIDITY_SENSOR_FAULT 0x00104EUL

#define IEC104_IOA_YC_485_TH_TEMP        0x002000UL
#define IEC104_IOA_YC_485_TH_HUMIDITY    0x002001UL
#define IEC104_IOA_YC_485_WIND_SPEED     0x002010UL
#define IEC104_IOA_YC_485_WIND_DIRECTION 0x002011UL
#define IEC104_IOA_YC_485_WATER_RAW      0x002020UL
#define IEC104_IOA_YC_485_RAIN_COUNT     0x002030UL
#define IEC104_IOA_YC_485_RAIN_INSTANT   0x002031UL
#define IEC104_IOA_YC_485_RAIN_DROPLET   0x002032UL
#define IEC104_IOA_YC_485_RAIN_WATER_STATE 0x002033UL
#define IEC104_IOA_YC_485_AC_CABINET_TEMP 0x002040UL
#define IEC104_IOA_YC_485_AC_EVAPORATOR_TEMP 0x002041UL
#define IEC104_IOA_YC_485_AC_CONDENSER_TEMP 0x002042UL
#define IEC104_IOA_YC_485_AC_HUMIDITY    0x002043UL

#define IEC104_IOA_YT_485_AC_COOLING_START 0x003000UL
#define IEC104_IOA_YT_485_AC_COOLING_STOP 0x003001UL
#define IEC104_IOA_YT_485_AC_HEATING_START 0x003002UL
#define IEC104_IOA_YT_485_AC_HEATING_STOP 0x003003UL
#define IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM 0x003004UL
#define IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM 0x003005UL
#define IEC104_IOA_YT_485_AC_CONDENSER_HIGH_PROTECT 0x003006UL
#define IEC104_IOA_YT_485_AC_EVAPORATOR_FREEZE_PROTECT 0x003007UL
#define IEC104_IOA_YT_485_AC_DEHUMIDIFY_START 0x003008UL
#define IEC104_IOA_YT_485_AC_DEHUMIDIFY_STOP 0x003009UL
#define IEC104_IOA_YT_485_AC_HUMIDITY_ALARM 0x00300AUL
#define IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION 0x00300BUL
#define IEC104_IOA_YT_485_AC_MODE_CONTROL 0x00300CUL

/* 一次采集保存所有业务模块数据，保证同一总召唤中的遥信和遥测来自同批快照。 */
typedef struct {
    module_moto_run_status_t motor;
    light_output_status_t lights;
    uint8_t ws2812_effect_active;
    xm7828_th_data_t th;
    xm8189_wind_data_t wind;
    xm8537_water_data_t water;
    xm9505_rain_data_t rain;
    ac_device_t ac;
} iec104_app_snapshot_t;

static uint16_t iec104_u16_read(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static void iec104_app_snapshot_read(iec104_app_snapshot_t *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    module_moto_sync_get_run_status(&snapshot->motor);
    light_output_get_status(&snapshot->lights);
    snapshot->ws2812_effect_active = module_ws2812_effect_is_active();
    (void)tf_modbus_xm7828_get(&snapshot->th);
    (void)tf_modbus_xm8189_get(&snapshot->wind);
    (void)tf_modbus_xm8537_get(&snapshot->water);
    (void)tf_modbus_xm9505_get(&snapshot->rain);
    (void)tf_modbus_ac_get(&snapshot->ac);
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

static uint8_t iec104_door_state_code(const module_moto_run_status_t *run_status)
{
    /* 工程约定：0 未知/中间，1 打开方向，2 关闭方向，3 故障。 */
    if (run_status->door == MODULE_MOTO_DOOR_STATE_FAULT) {
        return 3U;
    }
    if ((run_status->door == MODULE_MOTO_DOOR_STATE_OPENED) ||
        (module_moto_get_direction(DOOR1_MOTOR_ID) == MODULE_MOTO_DIR_FORWARD) ||
        (module_moto_get_direction(DOOR2_MOTOR_ID) == MODULE_MOTO_DIR_FORWARD)) {
        return 1U;
    }
    if ((run_status->door == MODULE_MOTO_DOOR_STATE_CLOSED) ||
        (module_moto_get_direction(DOOR1_MOTOR_ID) == MODULE_MOTO_DIR_BACKWARD) ||
        (module_moto_get_direction(DOOR2_MOTOR_ID) == MODULE_MOTO_DIR_BACKWARD)) {
        return 2U;
    }

    return 0U;
}

static uint8_t iec104_platform_state_code(const module_moto_run_status_t *run_status)
{
    /* 工程约定：0 未知/中间，1 伸出方向，2 收回方向，3 故障。 */
    if (run_status->platform == MODULE_MOTO_PLATFORM_STATE_FAULT) {
        return 3U;
    }
    if ((run_status->platform == MODULE_MOTO_PLATFORM_STATE_EXTENDED) ||
        (module_moto_get_direction(PLATFORM_MOTOR_ID) == MODULE_MOTO_DIR_FORWARD)) {
        return 1U;
    }
    if ((run_status->platform == MODULE_MOTO_PLATFORM_STATE_RETRACTED) ||
        (module_moto_get_direction(PLATFORM_MOTOR_ID) == MODULE_MOTO_DIR_BACKWARD)) {
        return 2U;
    }

    return 0U;
}

static uint8_t iec104_ac_has_fault(const ac_device_t *ac)
{
    const ac_status_t *status = &ac->run_status;

    return ac->status.sensor_fault || status->humidity_high_alarm ||
           status->condenser_high_pressure_alarm ||
           status->evaporator_freeze_alarm || status->cooling_alarm ||
           status->high_temp_alarm || status->low_temp_alarm;
}

static void iec104_app_fill_status_points(const iec104_app_snapshot_t *snapshot, iec104_status_point_t *points)
{
    const module_moto_run_status_t *run_status = &snapshot->motor;
    const light_output_status_t *lights = &snapshot->lights;
    const xm7828_th_data_t *th = &snapshot->th;
    const xm8189_wind_data_t *wind = &snapshot->wind;
    const xm8537_water_data_t *water = &snapshot->water;
    const xm9505_rain_data_t *rain = &snapshot->rain;
    const ac_device_t *ac = &snapshot->ac;
    uint8_t th_quality = iec104_status_quality(th->status.online, 0U);
    uint8_t wind_quality = iec104_status_quality(wind->status.online, 0U);
    uint8_t water_quality = iec104_status_quality(water->status.online, 0U);
    uint8_t rain_quality = iec104_status_quality(rain->status.online, 0U);
    uint8_t ac_quality = iec104_status_quality(ac->status.online, 0U);
    uint8_t door_running = (run_status->door == MODULE_MOTO_DOOR_STATE_INTERMEDIATE);
    uint8_t platform_running = (run_status->platform == MODULE_MOTO_PLATFORM_STATE_INTERMEDIATE);

    points[0] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_OPENED, (run_status->door == MODULE_MOTO_DOOR_STATE_OPENED), 0U };
    points[1] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_CLOSED, (run_status->door == MODULE_MOTO_DOOR_STATE_CLOSED), 0U };
    points[2] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_EXTENDED, (run_status->platform == MODULE_MOTO_PLATFORM_STATE_EXTENDED), 0U };
    points[3] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_RETRACTED, (run_status->platform == MODULE_MOTO_PLATFORM_STATE_RETRACTED), 0U };
    points[4] = (iec104_status_point_t){ IEC104_IOA_YX_EM_STOP, tf_key_is_pressed_now(TF_KEY_ID_EM_STOP), 0U };
    points[5] = (iec104_status_point_t){ IEC104_IOA_YX_DOOR_RUNNING, door_running, 0U };
    points[6] = (iec104_status_point_t){ IEC104_IOA_YX_HELIPED_RUNNING, platform_running, 0U };
    points[7] = (iec104_status_point_t){ IEC104_IOA_YX_WS2812_EFFECT_ACTIVE, snapshot->ws2812_effect_active, 0U };
    points[8] = (iec104_status_point_t){ IEC104_IOA_YX_INDICATOR_LIGHT_ON, lights->indicator_on, 0U };
    points[9] = (iec104_status_point_t){ IEC104_IOA_YX_INTERIOR_LIGHT_ON, lights->interior_light_on, 0U };
    points[10] = (iec104_status_point_t){ IEC104_IOA_YX_DECORATIVE_LIGHT_ON, lights->decorative_light_on, 0U };

    points[11] = (iec104_status_point_t){ IEC104_IOA_YX_485_TH_ONLINE, th->status.online, th_quality };
    points[12] = (iec104_status_point_t){ IEC104_IOA_YX_485_TH_FAULT, th->status.sensor_fault, th_quality };

    points[13] = (iec104_status_point_t){ IEC104_IOA_YX_485_WIND_ONLINE, wind->status.online, wind_quality };
    points[14] = (iec104_status_point_t){ IEC104_IOA_YX_485_WIND_FAULT, wind->status.sensor_fault, wind_quality };

    points[15] = (iec104_status_point_t){ IEC104_IOA_YX_485_WATER_ONLINE, water->status.online, water_quality };
    points[16] = (iec104_status_point_t){ IEC104_IOA_YX_485_WATER_ALARM, water->water_detected, water_quality };

    points[17] = (iec104_status_point_t){ IEC104_IOA_YX_485_RAIN_ONLINE, rain->status.online, rain_quality };
    points[18] = (iec104_status_point_t){ IEC104_IOA_YX_485_RAIN_FAULT, rain->status.sensor_fault, rain_quality };

    points[19] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_ONLINE, ac->status.online, ac_quality };
    points[20] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_FAULT, iec104_ac_has_fault(ac), ac_quality };
    points[21] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_COOLING, ac->run_status.cooling_on, ac_quality };
    points[22] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HEATING, ac->run_status.heater_on, ac_quality };
    points[23] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_INNER_FAN, ac->run_status.inner_fan_on, ac_quality };
    points[24] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_OUTER_FAN, ac->run_status.outer_fan_on, ac_quality };
    points[25] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HUMIDITY_HIGH, ac->run_status.humidity_high_alarm, ac_quality };
    points[26] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HIGH_TEMP, ac->run_status.high_temp_alarm, ac_quality };
    points[27] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_LOW_TEMP, ac->run_status.low_temp_alarm, ac_quality };
    points[28] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CONDENSER_HIGH, ac->run_status.condenser_high_pressure_alarm, ac_quality };
    points[29] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_EVAPORATOR_FREEZE, ac->run_status.evaporator_freeze_alarm, ac_quality };
    points[30] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CABINET_SENSOR_FAULT, ac->run_status.cabinet_sensor_fault, ac_quality };
    points[31] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_EVAPORATOR_SENSOR_FAULT, ac->run_status.evaporator_sensor_fault, ac_quality };
    points[32] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_CONDENSER_SENSOR_FAULT, ac->run_status.condenser_sensor_fault, ac_quality };
    points[33] = (iec104_status_point_t){ IEC104_IOA_YX_485_AC_HUMIDITY_SENSOR_FAULT, ac->run_status.humidity_sensor_fault, ac_quality };
}

static void iec104_app_fill_measure_points(const iec104_app_snapshot_t *snapshot, iec104_measure_point_t *points)
{
    const xm7828_th_data_t *th = &snapshot->th;
    const xm8189_wind_data_t *wind = &snapshot->wind;
    const xm8537_water_data_t *water = &snapshot->water;
    const xm9505_rain_data_t *rain = &snapshot->rain;
    const ac_device_t *ac = &snapshot->ac;
    uint8_t th_quality = iec104_status_quality(th->status.online, th->status.sensor_fault);
    uint8_t wind_quality = iec104_status_quality(wind->status.online, wind->status.sensor_fault);
    uint8_t water_quality = iec104_status_quality(water->status.online, water->status.sensor_fault);
    uint8_t rain_quality = iec104_status_quality(rain->status.online, rain->status.sensor_fault);
    uint8_t ac_cabinet_quality = iec104_status_quality(ac->status.online, ac->run_status.cabinet_sensor_fault);
    uint8_t ac_evaporator_quality = iec104_status_quality(ac->status.online, ac->run_status.evaporator_sensor_fault);
    uint8_t ac_condenser_quality = iec104_status_quality(ac->status.online, ac->run_status.condenser_sensor_fault);
    uint8_t ac_humidity_quality = iec104_status_quality(ac->status.online, ac->run_status.humidity_sensor_fault);

    points[0] = (iec104_measure_point_t){ IEC104_IOA_YC_DOOR_STATE,
        (float)iec104_door_state_code(&snapshot->motor), 0U };
    points[1] = (iec104_measure_point_t){ IEC104_IOA_YC_HELIPED_STATE,
        (float)iec104_platform_state_code(&snapshot->motor), 0U };
    points[2] = (iec104_measure_point_t){ IEC104_IOA_YC_485_TH_TEMP,
        (float)th->temperature_centi_c / 100.0F, th_quality };
    points[3] = (iec104_measure_point_t){ IEC104_IOA_YC_485_TH_HUMIDITY,
        (float)th->humidity_centi_rh / 100.0F, th_quality };
    points[4] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WIND_SPEED,
        (float)wind->speed_centi / 100.0F, wind_quality };
    points[5] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WIND_DIRECTION,
        (float)wind->direction_centi_deg / 100.0F, wind_quality };
    points[6] = (iec104_measure_point_t){ IEC104_IOA_YC_485_WATER_RAW,
        (float)water->raw, water_quality };
    points[7] = (iec104_measure_point_t){ IEC104_IOA_YC_485_RAIN_COUNT,
        (float)rain->rain_count, rain_quality };
    points[8] = (iec104_measure_point_t){ IEC104_IOA_YC_485_RAIN_INSTANT,
        (float)rain->instant_tenth_mm / 10.0F, rain_quality };
    points[9] = (iec104_measure_point_t){ IEC104_IOA_YC_485_RAIN_DROPLET,
        (float)rain->raindrop_dynamic, rain_quality };
    points[10] = (iec104_measure_point_t){ IEC104_IOA_YC_485_RAIN_WATER_STATE,
        (float)rain->water_state, rain_quality };
    points[11] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_CABINET_TEMP,
        (float)ac->params.cabinet_temp_deci_c / 10.0F, ac_cabinet_quality };
    points[12] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_EVAPORATOR_TEMP,
        (float)ac->params.evaporator_temp_deci_c / 10.0F, ac_evaporator_quality };
    points[13] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_CONDENSER_TEMP,
        (float)ac->params.condenser_temp_deci_c / 10.0F, ac_condenser_quality };
    points[14] = (iec104_measure_point_t){ IEC104_IOA_YC_485_AC_HUMIDITY,
        (float)ac->params.humidity_deci_rh / 10.0F, ac_humidity_quality };

    /* 参数区尚未收到有效寄存器数据时，四个空调遥测点统一标为无效。 */
    if (ac->params.marks.raw == 0U) {
        points[11].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[12].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[13].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
        points[14].quality = IEC104_QUALITY_IV | IEC104_QUALITY_NT;
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

static const iec104_command_desc_t iec104_command_table[] = {
    { IEC104_IOA_YK_DOOR_OPEN,       module_moto_business_request_door_open },
    { IEC104_IOA_YK_DOOR_CLOSE,      module_moto_business_request_door_close },
    { IEC104_IOA_YK_HELIPED_EXTEND,  module_moto_business_request_platform_extend },
    { IEC104_IOA_YK_HELIPED_RETRACT, module_moto_business_request_platform_retract },
    { IEC104_IOA_YK_INDICATOR_LIGHT_ON,  light_output_set_indicator_on },
    { IEC104_IOA_YK_INDICATOR_LIGHT_OFF, light_output_set_indicator_off },
    { IEC104_IOA_YK_INTERIOR_LIGHT_ON,   light_output_set_interior_light_on },
    { IEC104_IOA_YK_INTERIOR_LIGHT_OFF,  light_output_set_interior_light_off },
    { IEC104_IOA_YK_DECORATIVE_LIGHT_ON, light_output_set_decorative_light_on },
    { IEC104_IOA_YK_DECORATIVE_LIGHT_OFF, light_output_set_decorative_light_off },
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
    return (iec104_command_find(ioa) != NULL);
}

void iec104_app_remote_control_execute(uint32_t ioa)
{
    const iec104_command_desc_t *command = iec104_command_find(ioa);

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
} iec104_tuning_desc_t;

static const iec104_tuning_desc_t iec104_tuning_table[] = {
    { IEC104_IOA_YT_485_AC_COOLING_START,           AC_PARAM_REG_COOLING_START,          4U },
    { IEC104_IOA_YT_485_AC_COOLING_STOP,            AC_PARAM_REG_COOLING_STOP,           4U },
    { IEC104_IOA_YT_485_AC_HEATING_START,           AC_PARAM_REG_HEATING_START,          4U },
    { IEC104_IOA_YT_485_AC_HEATING_STOP,            AC_PARAM_REG_HEATING_STOP,           4U },
    { IEC104_IOA_YT_485_AC_HIGH_TEMP_ALARM,         AC_PARAM_REG_HIGH_TEMP_ALARM,        4U },
    { IEC104_IOA_YT_485_AC_LOW_TEMP_ALARM,          AC_PARAM_REG_LOW_TEMP_ALARM,         4U },
    { IEC104_IOA_YT_485_AC_CONDENSER_HIGH_PROTECT,  AC_PARAM_REG_CONDENSER_HIGH_PROTECT, 4U },
    { IEC104_IOA_YT_485_AC_EVAPORATOR_FREEZE_PROTECT, AC_PARAM_REG_EVAPORATOR_FREEZE,    4U },
    { IEC104_IOA_YT_485_AC_DEHUMIDIFY_START,        AC_PARAM_REG_DEHUMIDIFY_START,       4U },
    { IEC104_IOA_YT_485_AC_DEHUMIDIFY_STOP,         AC_PARAM_REG_DEHUMIDIFY_STOP,        4U },
    { IEC104_IOA_YT_485_AC_HUMIDITY_ALARM,          AC_PARAM_REG_HUMIDITY_ALARM,         4U },
    { IEC104_IOA_YT_485_AC_HUMIDITY_CALIBRATION,    AC_PARAM_REG_HUMIDITY_CALIBRATION,   4U },
    { IEC104_IOA_YT_485_AC_MODE_CONTROL,            AC_PARAM_REG_MODE_CONTROL,           2U },
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

uint8_t iec104_app_tuning_value_parse(uint32_t ioa, const uint8_t *data, uint32_t *value, uint8_t *value_length, uint16_t *register_value)
{
    float float_value;

    /* 模式控制在线路上是 2 字节整数，其余参数是 4 字节短浮点值。 */
    if (ioa == IEC104_IOA_YT_485_AC_MODE_CONTROL) {
        *value = iec104_u16_read(data);
        *value_length = 2U;
        *register_value = (uint16_t)*value;
        return (*register_value <= 0x7FFFU);
    }

    memcpy(&float_value, data, sizeof(float_value));
    if ((float_value != float_value) ||
        (float_value < -3276.8F) || (float_value > 3276.7F)) {
        return 0U;
    }

    memcpy(value, data, sizeof(float_value));
    *value_length = 4U;
    /* 空调寄存器以 0.1 为单位保存有符号值。 */
    *register_value = (uint16_t)(int16_t)(float_value * 10.0F);
    return 1U;
}

uint8_t iec104_app_tuning_write(uint32_t ioa, uint16_t value)
{
    ac_device_t ac;
    uint16_t register_address;

    memset(&ac, 0, sizeof(ac));
    if (!tf_modbus_ac_get(&ac)) {
        return 0U;
    }

    register_address = iec104_app_tuning_register(ioa);
    return (register_address != 0U) &&
           tf_modbus_ac_write_param(ac.slave_addr, register_address, (int16_t)value);
}
