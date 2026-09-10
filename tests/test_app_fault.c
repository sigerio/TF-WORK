/* Run with: python3 tests/check_app_safety.py */
#include <assert.h>
#include "APP/app_fault.h"
#include "COMM/modbus/modbus_devices.h"
#include "COMM/modbus/modbus_master.h"
#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"

static motor_control_status_t motor;
static xm7828_th_data_t th;
static xm8189_wind_data_t wind;
static xm8537_water_data_t water;
static xm9505_rain_data_t rain;
static ac_device_t ac;
static bool estop;
static unsigned publications;
static uint32_t published_fault;

void modbus_master_queue_weather_fault(uint32_t value) { published_fault = value; publications++; }

void motor_control_get_status(motor_control_status_t *out) { *out = motor; }
bool key_is_pressed_now(key_id_t key) { assert(key == KEY_ID_EM_STOP); return estop; }
bool modbus_xm7828_get(xm7828_th_data_t *out) { *out = th; return true; }
bool modbus_xm8189_get(xm8189_wind_data_t *out) { *out = wind; return true; }
bool modbus_xm8537_get(xm8537_water_data_t *out) { *out = water; return true; }
bool modbus_xm9505_get(xm9505_rain_data_t *out) { *out = rain; return true; }
bool modbus_ac_get(ac_device_t *out) { *out = ac; return ac.status.online; }

static void check(uint32_t expected, app_fault_priority_e priority)
{
    uint32_t previous = app_fault_get().raw;
    unsigned before = publications;
    app_fault_update();
    app_fault_u faults = app_fault_get();
    assert(faults.raw == expected);
    assert(app_fault_highest_priority(faults) == priority);
    assert(publications == before + (previous != expected));
    assert(published_fault == expected);
    app_fault_update();
    assert(publications == before + (previous != expected)); /* Stable samples do not keep requesting transmission. */
}

/* Each source must set exactly its documented bit and clear when it recovers. */
#define CHECK_SOURCE(source, value, field, bit, priority) do { \
    source = value; \
    check(1UL << bit, priority); \
    assert(app_fault_get().bits.field == 1U); \
    source = 0; \
    check(0U, APP_FAULT_PRIORITY_NONE); \
} while (0)

#define CHECK_OFFLINE(device, bit) do { \
    device.status.online = false; \
    check(1UL << bit, APP_FAULT_PRIORITY_P3); \
    device.status.online = true; \
    check(0U, APP_FAULT_PRIORITY_NONE); \
} while (0)

int main(void)
{
    assert(sizeof(app_fault_u) == sizeof(uint32_t));
    /* No response at boot is monitoring unavailable, not proof of a safe environment. */
    check(0x0001F000UL, APP_FAULT_PRIORITY_P3);
    th.status.online = wind.status.online = water.status.online = rain.status.online = ac.status.online = true;
    check(0U, APP_FAULT_PRIORITY_NONE);

    CHECK_SOURCE(estop, true, emergency_stop, 0, APP_FAULT_PRIORITY_P1);
    CHECK_SOURCE(motor.door_fault, MOTOR_FAULT_LIMIT_CONFLICT, door_limit_conflict, 1, APP_FAULT_PRIORITY_P1);
    CHECK_SOURCE(motor.platform_fault, MOTOR_FAULT_LIMIT_CONFLICT, platform_limit_conflict, 2, APP_FAULT_PRIORITY_P1);
    CHECK_SOURCE(water.water_detected, true, water_detected, 3, APP_FAULT_PRIORITY_P1);
    CHECK_SOURCE(motor.door_fault, MOTOR_FAULT_START_FAILED, door_start_failed, 4, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(motor.platform_fault, MOTOR_FAULT_START_FAILED, platform_start_failed, 5, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.high_temp_alarm, true, ac_high_temp, 6, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.low_temp_alarm, true, ac_low_temp, 7, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.humidity_high_alarm, true, ac_humidity_high, 8, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.condenser_high_pressure_alarm, true, ac_condenser_high_pressure, 9, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.evaporator_freeze_alarm, true, ac_evaporator_freeze, 10, APP_FAULT_PRIORITY_P2);
    CHECK_SOURCE(ac.run_status.cooling_alarm, true, ac_cooling_alarm, 11, APP_FAULT_PRIORITY_P2);
    CHECK_OFFLINE(th, 12);
    CHECK_OFFLINE(wind, 13);
    CHECK_OFFLINE(water, 14);
    CHECK_OFFLINE(rain, 15);
    CHECK_OFFLINE(ac, 16);
    CHECK_SOURCE(th.status.sensor_fault, true, th_sensor_fault, 17, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(wind.status.sensor_fault, true, wind_sensor_fault, 18, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(water.status.sensor_fault, true, water_sensor_fault, 19, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(rain.status.sensor_fault, true, rain_sensor_fault, 20, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(ac.run_status.cabinet_sensor_fault, true, ac_cabinet_sensor_fault, 21, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(ac.run_status.evaporator_sensor_fault, true, ac_evaporator_sensor_fault, 22, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(ac.run_status.condenser_sensor_fault, true, ac_condenser_sensor_fault, 23, APP_FAULT_PRIORITY_P3);
    CHECK_SOURCE(ac.run_status.humidity_sensor_fault, true, ac_humidity_sensor_fault, 24, APP_FAULT_PRIORITY_P3);

    /* Water reading is stale after invalid measurement or disconnect. */
    water.water_detected = true;
    water.status.sensor_fault = true;
    check(1UL << 19, APP_FAULT_PRIORITY_P3);
    water.status.online = false;
    check(1UL << 14, APP_FAULT_PRIORITY_P3);
    water.status.online = true;
    water.status.sensor_fault = false;
    check(1UL << 3, APP_FAULT_PRIORITY_P1);
    water.water_detected = false;

    /* Keep all simultaneous faults, then downgrade as their sources recover. */
    motor.door_fault = MOTOR_FAULT_LIMIT_CONFLICT | MOTOR_FAULT_START_FAILED;
    ac.run_status.cooling_alarm = true;
    ac.run_status.cabinet_sensor_fault = true;
    check((1UL << 1) | (1UL << 4) | (1UL << 11) | (1UL << 21), APP_FAULT_PRIORITY_P1);
    app_fault_u saved = app_fault_get();
    motor.door_fault = MOTOR_FAULT_NONE;
    assert(app_fault_get().raw == saved.raw); /* Getter only returns the last sample. */
    check((1UL << 11) | (1UL << 21), APP_FAULT_PRIORITY_P2);
    assert(saved.bits.door_limit_conflict && saved.bits.door_start_failed);
    ac.status.online = false;
    check(1UL << 16, APP_FAULT_PRIORITY_P3); /* Stale AC alarms are not current alarms. */
    ac.status.online = true;
    ac.run_status.cooling_alarm = false;
    ac.run_status.cabinet_sensor_fault = false;
    check(0U, APP_FAULT_PRIORITY_NONE);
    return 0;
}
