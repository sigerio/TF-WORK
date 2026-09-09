/* Run with: python3 tests/check_app_safety.py */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "APP/tasks/app_timer_task.h"
#include "APP/app_events.h"
#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"
#include "CONTROL/motor/motor_driver.h"
#include "BOARD/board.h"

static struct {
    void (*callback)(void *);
    void *arg;
    uint32_t period;
} timers[8];
static unsigned timer_count, requests, stops, key_scans;
static bool pressed;
static bool moving[MOTOR_ID_COUNT];
static bool fail_start;
static uint16_t led_period;
static uint8_t limit_faults;
static unsigned reports;

void os_soft_timer_task(void (*callback)(void *), void *arg, uint32_t period_ms, uint8_t priority)
{
    (void)priority;
    assert(timer_count < 8);
    timers[timer_count].callback = callback;
    timers[timer_count].arg = arg;
    timers[timer_count++].period = period_ms;
}

bool key_is_pressed_now(key_id_t key) { assert(key == KEY_ID_EM_STOP); return pressed; }
void key_scan_task(uint32_t cycle_ms) { assert(cycle_ms == 50U); key_scans++; }
key_info_t key_get_info(void) { key_info_t info = {0}; return info; }
int8_t os_event_trigger(uint8_t event, const void *arg, uint16_t size, uint8_t count)
{
    (void)arg; (void)size; (void)count;
    assert(event != USR_EVENT_EM_STOP && event != USR_EVENT_EM_STOP_RECOVER);
    return 0;
}

void modbus_master_task(void) {}
void modbus_devices_poll(void) {}
void iec104_server_comm_task(void) {}
void iec104_server_report_task(void) { reports++; }
void debug_console_task(void) {}
void module_ws2812_status_update(uint16_t cycle_ms) { led_period = cycle_ms; }

void board_motor_io_init(void) {}
void board_door_lock_write(bool locked) { (void)locked; }
void board_motor_read_limits(board_motor_limits_t *limits)
{
    *limits = (board_motor_limits_t){0};
    limits->door_closed = limits->door_opened = (limit_faults & 1U) != 0;
    limits->platform_extended = limits->platform_retracted = (limit_faults & 2U) != 0;
}
bool motor_driver_init(void) { return true; }
bool motor_driver_brake(motor_id_e motor) { (void)motor; return true; }
bool motor_driver_start(motor_id_e motor, motor_direction_e direction)
{
    (void)direction;
    assert(!pressed); /* Physical estop must be processed before any output starts. */
    requests++;
    moving[motor] = !fail_start;
    return !fail_start;
}
bool motor_driver_stop(motor_id_e motor) { stops++; moving[motor] = false; return true; }

static void request_all_commands(void)
{
    motor_control_request_door_open();
    motor_control_request_door_close();
    motor_control_request_platform_extend();
    motor_control_request_platform_retract();
    motor_control_toggle_door();
    motor_control_toggle_platform();
}

static void assert_all_stopped(void)
{
    assert(!moving[0] && !moving[1] && !moving[2]);
}

int main(int argc, char **argv)
{
    motor_control_status_t status;
    assert(argc == 2);
    motor_control_init();
    pressed = atoi(argv[1]) != 0;
    usr_time_task_init();
    assert(timer_count == 8 && timers[4].period == 50U && timers[5].period == 10U);
    if (pressed) {
        request_all_commands();
        assert(requests == 0);
    }

    pressed = false;
    timers[5].callback(timers[5].arg);
    assert_all_stopped();
    motor_control_request_door_open();
    motor_control_request_platform_retract();
    timers[5].callback(timers[5].arg);
    assert(requests == 3 && moving[0] && moving[1] && moving[2]);

    unsigned stops_before = stops;
    pressed = true;
    timers[5].callback(timers[5].arg);
    assert(stops == stops_before + 3 && key_scans == 0);
    assert_all_stopped();
    request_all_commands();
    timers[5].callback(timers[5].arg);
    assert(requests == 3 && stops == stops_before + 3);

    pressed = false;
    for (unsigned i = 0; i < 20; i++) timers[5].callback(timers[5].arg);
    assert(requests == 3);
    assert_all_stopped();
    motor_control_request_door_open();
    timers[5].callback(timers[5].arg);
    assert(requests == 5 && moving[1] && moving[2]);

    timers[4].callback(timers[4].arg);
    timers[6].callback(timers[6].arg);
    assert(key_scans == 1 && led_period == 50U);
    limit_faults = 3;
    timers[5].callback(timers[5].arg);
    assert(reports == 1);
    assert_all_stopped();
    timers[5].callback(timers[5].arg);
    assert(reports == 1);
    limit_faults = 0;
    timers[5].callback(timers[5].arg);
    assert(reports == 2);
    assert_all_stopped();

    /* Three failed attempts stop the mechanism; a new command may retry. */
    fail_start = true;
    motor_control_request_door_open();
    motor_control_request_platform_retract();
    for (unsigned i = 0; i < 4; i++) timers[5].callback(timers[5].arg);
    motor_control_get_status(&status);
    assert(status.door_fault == MOTOR_FAULT_START_FAILED && status.platform_fault == MOTOR_FAULT_START_FAILED);
    assert_all_stopped();
    fail_start = false;
    motor_control_request_door_open();
    motor_control_request_platform_retract();
    timers[5].callback(timers[5].arg);
    motor_control_get_status(&status);
    assert(status.door_fault == MOTOR_FAULT_NONE && status.platform_fault == MOTOR_FAULT_NONE);
    assert(moving[0] && moving[1] && moving[2]);
    return 0;
}
