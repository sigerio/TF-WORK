#include "APP/tasks/app_event_task.h"
#include "APP/app_events.h"

#include "COMM/modbus/modbus_devices.h"
#include "OS/os.h"
#include "CONTROL/key/key_event.h"
#include "CONTROL/motor/motor_control.h"
#include "CONTROL/ws2812/ws2812_status.h"
#include "DEBUG/debug_log.h"


typedef struct {
    uint8_t slave_addr;
    uint16_t ref;
    uint16_t value;
} usr_485_write_arg_t;

static void usr_485_xm7828_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    (void)modbus_xm7828_queue_write(write_arg->slave_addr, write_arg->ref, write_arg->value);
}

static void usr_485_xm8189_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    (void)modbus_xm8189_queue_write(write_arg->slave_addr, write_arg->ref, write_arg->value);
}

static void usr_485_xm8537_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    (void)modbus_xm8537_queue_write(write_arg->slave_addr, write_arg->ref, write_arg->value);
}

static void usr_485_xm9505_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    (void)modbus_xm9505_queue_write(write_arg->slave_addr, write_arg->ref, write_arg->value);
}

static void usr_485_ac_status_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!modbus_ac_queue_write_status(write_arg->slave_addr, write_arg->ref, write_arg->value != 0U)) {
        DEBUG("EVENT", "ac status write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
            (unsigned)write_arg->slave_addr, (unsigned)write_arg->ref, (unsigned)write_arg->value);
    }
}

static void usr_485_ac_param_write_cb(void *arg)
{
    const usr_485_write_arg_t *write_arg = (const usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!modbus_ac_queue_write_param(write_arg->slave_addr, write_arg->ref, (int16_t)write_arg->value)) {
        DEBUG("EVENT", "ac param write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
            (unsigned)write_arg->slave_addr, (unsigned)write_arg->ref, (unsigned)write_arg->value);
    }
}


static void usr_key_event_c(void *arg)
{
    (void)arg;
    key_event_callback();
}

static void usr_em_stop_event_cb(void *arg)
{
    (void)arg;
    motor_control_set_estop(true);
}

static void usr_em_stop_recover_event_cb(void *arg)
{
    (void)arg;
    motor_control_set_estop(false);
}


static void usr_door_state_event_cb(void *arg)
{
    const door_running_in_e *running = (const door_running_in_e *)arg;

    if (running == 0) {
        return;
    }

    set_door_running_in(*running);
}

static void usr_platform_state_event_cb(void *arg)
{
    const platform_running_in_e *running = (const platform_running_in_e *)arg;

    if (running == 0) {
        return;
    }

    set_platform_running_in(*running);
}


void usr_event_task_init(void)
{
    os_soft_event_task(USR_EVENT_485_XM7828_WRITE, usr_485_xm7828_write_cb, 20);
    os_soft_event_task(USR_EVENT_485_XM8189_WRITE, usr_485_xm8189_write_cb, 20);
    os_soft_event_task(USR_EVENT_485_XM8537_WRITE, usr_485_xm8537_write_cb, 20);
    os_soft_event_task(USR_EVENT_485_XM9505_WRITE, usr_485_xm9505_write_cb, 20);
    os_soft_event_task(USR_EVENT_485_AC_STATUS_WRITE, usr_485_ac_status_write_cb, 20);
    os_soft_event_task(USR_EVENT_485_AC_PARAM_WRITE, usr_485_ac_param_write_cb, 20);

    os_soft_event_task(USR_EVENT_EM_STOP, usr_em_stop_event_cb, 0);
    os_soft_event_task(USR_EVENT_EM_STOP_RECOVER, usr_em_stop_recover_event_cb, 0);
    os_soft_event_task(USR_EVENT_KEY_TRIG, usr_key_event_c, 1);
    os_soft_event_task(USR_EVENT_DOOR_STATE, usr_door_state_event_cb, 2);
    os_soft_event_task(USR_EVENT_PLATFORM_STATE, usr_platform_state_event_cb, 2);
}


// arg.slave_addr = slave_addr;
// arg.ref = ref;
// arg.value = value;

// return os_event_trigger(event_id, &arg, sizeof(arg), 1U);
