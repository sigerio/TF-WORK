#include "APP/tasks/app_timer_task.h"

#include "OS/os.h"
#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"
#include "COMM/iec104/iec104_server.h"
#include "COMM/modbus/modbus_master.h"
#include "COMM/modbus/modbus_devices.h"
#include "CONTROL/ws2812/ws2812_status.h"
#include "APP/app_events.h"
#include "DEBUG/debug_console.h"

#define KEY_SCAN_PERIOD_MS 50U

static const uint32_t key_scan_period_ms = KEY_SCAN_PERIOD_MS;
static const uint32_t ws2812_period_ms = MODULE_WS2812_PERIOD_MS;

/* 启动时及每次运动更新前读取物理急停，不等待普通按键扫描或软事件处理。 */
static void update_estop_state(void)
{
    if (key_is_pressed_now(KEY_ID_EM_STOP)) {
        motor_control_set_estop(true);
    } else {
        motor_control_set_estop(false);
    }
}

static void modbus_master_timer_task(void *arg)
{
    (void)arg;
    modbus_master_task();
}

static void modbus_poll_timer_task(void *arg)
{
    (void)arg;
    modbus_devices_poll();
}

static void iec104_server_comm_timer_task(void *arg)
{
    (void)arg;
    iec104_server_comm_task(); //解帧，ack，以及发送
}

static void iec104_server_report_timer_task(void *arg)
{
    (void)arg;
    iec104_server_report_task(); //变位上传
}

static void key_scan_timer_task(void *arg)
{
    const uint32_t *period_ms = (const uint32_t *)arg;
    uint32_t cycle_ms = (period_ms != 0) ? *period_ms : KEY_SCAN_PERIOD_MS;
    key_info_t key_info;

    key_scan_task(cycle_ms);

    key_info = key_get_info();

    for (int i = 0; i < KEY_ID_COUNT; i++) {
        if (i == KEY_ID_EM_STOP) {
            continue;
        }

        if (key_info.key_event[i] != KEY_EVENT_RESET) {
            (void)os_event_trigger(USR_EVENT_KEY_TRIG, 0, 0U, 1U);
            return;
        }
    }
}

static void motor_control_update_timer_task(void *arg)
{
    static uint8_t previous_limit_faults;
    motor_control_status_t status;
    uint8_t limit_faults;

    (void)arg;
    update_estop_state();
    motor_control_update();
    motor_control_get_status(&status);
    limit_faults = ((status.door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) | (((status.platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) << 1U);
    if (limit_faults != previous_limit_faults) {
        /* 停机后立即触发现有变位上报，不等待 100 ms 常规报告周期。 */
        iec104_server_report_task();
        previous_limit_faults = limit_faults;
    }
}


static void module_ws2812_run_timer_task(void *arg)
{
    const uint32_t *cycle_ms = (const uint32_t *)arg;
    module_ws2812_status_update((uint16_t)*cycle_ms);
}

static void debug_console_timer_task(void *arg)
{
    (void)arg;
    debug_console_task();
}

void usr_time_task_init(void)
{
    update_estop_state();
    os_soft_timer_task(modbus_master_timer_task, 0, MODULE_MODBUS_MASTER_PERIOD_MS, 10U);
    os_soft_timer_task(modbus_poll_timer_task, 0, MODULE_MODBUS_QUEUE_FILL_PERIOD_MS, 20U);
    os_soft_timer_task(iec104_server_comm_timer_task, 0, IEC104_SERVER_COMM_PERIOD_MS, 10U);
    os_soft_timer_task(iec104_server_report_timer_task, 0, IEC104_SERVER_REPORT_PERIOD_MS, 15U);
    os_soft_timer_task(key_scan_timer_task, (void *)&key_scan_period_ms, KEY_SCAN_PERIOD_MS, 30U);
    os_soft_timer_task(motor_control_update_timer_task, 0, MOTOR_CONTROL_PERIOD_MS, 2U);
    os_soft_timer_task(module_ws2812_run_timer_task, (void*)&ws2812_period_ms, MODULE_WS2812_PERIOD_MS, 30U);
    os_soft_timer_task(debug_console_timer_task, 0, 10U, 5U);
}
