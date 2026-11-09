#include "APP/app_fault.h"

#include "COMM/modbus/modbus_devices.h"
#include "COMM/modbus/modbus_master.h"
#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"

static app_fault_u fault_state;

void app_fault_update(void)
{
    app_fault_u faults = {0};
    motor_control_status_t motor;
    xm8537_water_data_t water;
    ac_device_t ac;
    weather_sensor_data_t weather;

    motor_control_get_status(&motor);
    (void)modbus_xm8537_get(&water);
    (void)modbus_ac_get(&ac);
    (void)modbus_weather_sensor_get(&weather);

    faults.bits.emergency_stop = key_is_pressed_now(KEY_ID_EM_STOP);
    faults.bits.door_limit_conflict = (motor.door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0;
    faults.bits.platform_limit_conflict = (motor.platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0;
    faults.bits.water_detected = water.status.online && !water.status.sensor_fault && water.water_detected;
    faults.bits.door_start_failed = (motor.door_fault & MOTOR_FAULT_START_FAILED) != 0;
    faults.bits.platform_start_failed = (motor.platform_fault & MOTOR_FAULT_START_FAILED) != 0;

    /* 离线缓存只表示上次读数，不能继续作为当前有效的空调告警。 */
    faults.bits.ac_high_temp = ac.status.online && ac.run_status.high_temp_alarm;
    faults.bits.ac_low_temp = ac.status.online && ac.run_status.low_temp_alarm;
    faults.bits.ac_humidity_high = ac.status.online && ac.run_status.humidity_high_alarm;
    faults.bits.ac_condenser_high_pressure = ac.status.online && ac.run_status.condenser_high_pressure_alarm;
    faults.bits.ac_evaporator_freeze = ac.status.online && ac.run_status.evaporator_freeze_alarm;
    faults.bits.ac_cooling_alarm = ac.status.online && ac.run_status.cooling_alarm;

    faults.bits.th_offline = !weather.status.online;
    faults.bits.wind_offline = !weather.status.online;
    faults.bits.water_offline = !water.status.online;
    faults.bits.rain_offline = !weather.status.online;
    faults.bits.ac_offline = !ac.status.online;
    faults.bits.th_sensor_fault = weather.status.online && weather.th_fault;
    faults.bits.wind_sensor_fault = weather.status.online && weather.wind_fault;
    faults.bits.water_sensor_fault = water.status.online && water.status.sensor_fault;
    faults.bits.rain_sensor_fault = false;
    faults.bits.ac_cabinet_sensor_fault = ac.status.online && ac.run_status.cabinet_sensor_fault;
    faults.bits.ac_evaporator_sensor_fault = ac.status.online && ac.run_status.evaporator_sensor_fault;
    faults.bits.ac_condenser_sensor_fault = ac.status.online && ac.run_status.condenser_sensor_fault;
    faults.bits.ac_humidity_sensor_fault = ac.status.online && ac.run_status.humidity_sensor_fault;
    if (fault_state.raw != faults.raw) {
        fault_state = faults;
        modbus_master_queue_weather_fault(faults.raw);
    }
}

app_fault_u app_fault_get(void)
{
    return fault_state;
}

app_fault_priority_e app_fault_highest_priority(app_fault_u faults)
{
    if (faults.levels.p1 != 0U) return APP_FAULT_PRIORITY_P1;
    if (faults.levels.p2 != 0U) return APP_FAULT_PRIORITY_P2;
    if (faults.levels.p3 != 0U) return APP_FAULT_PRIORITY_P3;
    return APP_FAULT_PRIORITY_NONE;
}
