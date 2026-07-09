#include <stdint.h>

#include "../../TFSDK/framework/os/tf_os.h"
#include "tf_usr_event_task.h"
#include "../MODULE/module_modbus/tf_485_ac.h"
#include "../MODULE/module_modbus/tf_485_device_common.h"
#include "../MODULE/module_modbus/tf_485_xm7828.h"
#include "../MODULE/module_modbus/tf_485_xm8189.h"
#include "../MODULE/module_modbus/tf_485_xm8537.h"
#include "../MODULE/module_modbus/tf_485_xm9505.h"
#include "tf_debug.h"

#ifndef MODULE_MODBUS_XM7828_ADDR
#define MODULE_MODBUS_XM7828_ADDR              1U
#endif

#ifndef MODULE_MODBUS_XM8189_ADDR
#define MODULE_MODBUS_XM8189_ADDR              2U
#endif

#ifndef MODULE_MODBUS_XM8537_ADDR
#define MODULE_MODBUS_XM8537_ADDR              3U
#endif

#ifndef MODULE_MODBUS_XM9505_ADDR
#define MODULE_MODBUS_XM9505_ADDR              4U
#endif

#ifndef MODULE_MODBUS_AC_ADDR
#define MODULE_MODBUS_AC_ADDR                  5U
#endif

#ifndef TF_USR_EVENT_TASK_PRIORITY
#define TF_USR_EVENT_TASK_PRIORITY             20U
#endif

typedef struct tf_usr_modbus_event_task {
    uint8_t event_id;
    uint8_t slave_addr;
    uint16_t ref;
    uint16_t value;
    bool (*write)(const struct tf_usr_modbus_event_task *task);
} tf_usr_modbus_event_task_t;

static bool tf_usr_modbus_write_xm7828(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_xm7828_write(task->slave_addr,
                               task->ref,
                               (uint16_t)task->value);
}

static bool tf_usr_modbus_write_xm8189(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_xm8189_write(task->slave_addr,
                               task->ref,
                               (uint16_t)task->value);
}

static bool tf_usr_modbus_write_xm8537(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_xm8537_write(task->slave_addr,
                               task->ref,
                               (uint16_t)task->value);
}

static bool tf_usr_modbus_write_xm9505(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_xm9505_write(task->slave_addr,
                               task->ref,
                               (uint16_t)task->value);
}

static bool tf_usr_modbus_write_ac_status(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_ac_write_status(task->slave_addr,
                                  task->ref,
                                  task->value != 0);
}

static bool tf_usr_modbus_write_ac_param(
    const tf_usr_modbus_event_task_t *task)
{
    return tf_485_ac_write_param(task->slave_addr,
                                 task->ref,
                                 (int16_t)task->value);
}

static tf_usr_modbus_event_task_t s_modbus_write_tasks[] = {
    {
        TF_USR_EVENT_MODBUS_XM7828_WRITE,
        MODULE_MODBUS_XM7828_ADDR,
        0U,
        0,
        tf_usr_modbus_write_xm7828,
    },
    {
        TF_USR_EVENT_MODBUS_XM8189_WRITE,
        MODULE_MODBUS_XM8189_ADDR,
        0U,
        0,
        tf_usr_modbus_write_xm8189,
    },
    {
        TF_USR_EVENT_MODBUS_XM8537_WRITE,
        MODULE_MODBUS_XM8537_ADDR,
        0U,
        0,
        tf_usr_modbus_write_xm8537,
    },
    {
        TF_USR_EVENT_MODBUS_XM9505_WRITE,
        MODULE_MODBUS_XM9505_ADDR,
        0U,
        0,
        tf_usr_modbus_write_xm9505,
    },
    {
        TF_USR_EVENT_MODBUS_AC_STATUS_WRITE,
        MODULE_MODBUS_AC_ADDR,
        AC_STATUS_POWER_MONITOR,
        0,
        tf_usr_modbus_write_ac_status,
    },
    {
        TF_USR_EVENT_MODBUS_AC_PARAM_WRITE,
        MODULE_MODBUS_AC_ADDR,
        0U,
        0,
        tf_usr_modbus_write_ac_param,
    },
};

static tf_usr_modbus_event_task_t *tf_usr_event_find_write_task(
    uint8_t event_id)
{
    uint8_t i;

    for (i = 0U;
         i < (uint8_t)(sizeof(s_modbus_write_tasks) /
                       sizeof(s_modbus_write_tasks[0]));
         i++) {
        if (s_modbus_write_tasks[i].event_id == event_id) {
            return &s_modbus_write_tasks[i];
        }
    }

    return 0;
}

static void tf_usr_modbus_write_event_cb(void *arg)
{
    const tf_usr_modbus_event_task_t *task =
        (const tf_usr_modbus_event_task_t *)arg;
    bool queued;

    if ((task == 0) || (task->write == 0)) {
        return;
    }

    queued = task->write(task);
    if (!queued) {
        TF_DEBUG("EVENT",
                 "modbus write enqueue failed event=%u slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)task->event_id,
                 (unsigned)task->slave_addr,
                 (unsigned)task->ref,
                 (unsigned)task->value);
    }
}

void tf_usr_event_task_init(void)
{
    uint8_t i;

    for (i = 0U;
         i < (uint8_t)(sizeof(s_modbus_write_tasks) /
                       sizeof(s_modbus_write_tasks[0]));
         i++) {
        tf_os_soft_event_task(s_modbus_write_tasks[i].event_id,
                              tf_usr_modbus_write_event_cb,
                              (void *)&s_modbus_write_tasks[i],
                              TF_USR_EVENT_TASK_PRIORITY);
    }
}

int8_t tf_usr_event_task_post_write(uint8_t event_id,
                                    uint16_t ref,
                                    uint16_t value)
{
    tf_usr_modbus_event_task_t *task = tf_usr_event_find_write_task(event_id);

    if (task == 0) {
        return -1;
    }

    task->ref = ref;
    task->value = value;
    tf_os_event_trigger(event_id, 1U);

    return 0;
}

void tf_usr_event_task_trigger(uint8_t event_id)
{
    tf_os_event_trigger(event_id, 1U);
}
