#include "tf_usr_event_task.h"

#include "../../TFSDK/framework/os/tf_os.h"
#include "../MODULE/module_modbus/tf_485_ac.h"
#include "../MODULE/module_modbus/tf_485_xm7828.h"
#include "../MODULE/module_modbus/tf_485_xm8189.h"
#include "../MODULE/module_modbus/tf_485_xm8537.h"
#include "../MODULE/module_modbus/tf_485_xm9505.h"
#include "../MODULE/KEY/key_event.h"
#include "tf_debug.h"


typedef struct {
    uint8_t slave_addr;
    uint16_t ref;
    uint16_t value;
} tf_usr_485_write_arg_t;

static void tf_usr_485_xm7828_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_xm7828_write(write_arg->slave_addr,
                             write_arg->ref,
                             write_arg->value)) {
        TF_DEBUG("EVENT",
                 "xm7828 write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}

static void tf_usr_485_xm8189_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_xm8189_write(write_arg->slave_addr,
                             write_arg->ref,
                             write_arg->value)) {
        TF_DEBUG("EVENT",
                 "xm8189 write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}

static void tf_usr_485_xm8537_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_xm8537_write(write_arg->slave_addr,
                             write_arg->ref,
                             write_arg->value)) {
        TF_DEBUG("EVENT",
                 "xm8537 write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}

static void tf_usr_485_xm9505_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_xm9505_write(write_arg->slave_addr,
                             write_arg->ref,
                             write_arg->value)) {
        TF_DEBUG("EVENT",
                 "xm9505 write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}

static void tf_usr_485_ac_status_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_ac_write_status(write_arg->slave_addr,
                                write_arg->ref,
                                write_arg->value != 0U)) {
        TF_DEBUG("EVENT",
                 "ac status write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}

static void tf_usr_485_ac_param_write_cb(void *arg)
{
    const tf_usr_485_write_arg_t *write_arg =
        (const tf_usr_485_write_arg_t *)arg;

    if (write_arg == 0) {
        return;
    }

    if (!tf_485_ac_write_param(write_arg->slave_addr,
                               write_arg->ref,
                               (int16_t)write_arg->value)) {
        TF_DEBUG("EVENT",
                 "ac param write enqueue failed slave=%u ref=0x%04X value=0x%04X\n",
                 (unsigned)write_arg->slave_addr,
                 (unsigned)write_arg->ref,
                 (unsigned)write_arg->value);
    }
}


static void tf_usr_key_event_c(void *arg)
{
    (void)arg;
    tf_key_event_callback();
}


void tf_usr_event_task_init(void)
{
    tf_os_soft_event_task(TF_USR_EVENT_485_XM7828_WRITE,
                          tf_usr_485_xm7828_write_cb,
                          20);
    tf_os_soft_event_task(TF_USR_EVENT_485_XM8189_WRITE,
                          tf_usr_485_xm8189_write_cb,
                          20);
    tf_os_soft_event_task(TF_USR_EVENT_485_XM8537_WRITE,
                          tf_usr_485_xm8537_write_cb,
                          20);
    tf_os_soft_event_task(TF_USR_EVENT_485_XM9505_WRITE,
                          tf_usr_485_xm9505_write_cb,
                          20);
    tf_os_soft_event_task(TF_USR_EVENT_485_AC_STATUS_WRITE,
                          tf_usr_485_ac_status_write_cb,
                          20);
    tf_os_soft_event_task(TF_USR_EVENT_485_AC_PARAM_WRITE,
                          tf_usr_485_ac_param_write_cb,
                          20);

    tf_os_soft_event_task(TF_USR_EVENT_KEY_TRIG,
                          tf_usr_key_event_c,
                          0);
}


// arg.slave_addr = slave_addr;
// arg.ref = ref;
// arg.value = value;

// return tf_os_event_trigger(event_id, &arg, sizeof(arg), 1U);
