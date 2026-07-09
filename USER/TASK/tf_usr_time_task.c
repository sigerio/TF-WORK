#include "tf_usr_time_task.h"

#include "../../TFSDK/framework/os/tf_os.h"
#include "../MODULE/module_modbus/tf_485_master.h"

static void tf_485_master_timer_task(void *arg)
{
    (void)arg;
    tf_485_master_task();
}

static void tf_485_query_timer_task(void *arg)
{
    (void)arg;
    tf_485_query_task();
}

void tf_usr_time_task_init(void)
{
    tf_os_soft_timer_task(tf_485_master_timer_task,
                          0,
                          MODULE_MODBUS_MASTER_PERIOD_MS,
                          10U);
    tf_os_soft_timer_task(tf_485_query_timer_task,
                          0,
                          MODULE_MODBUS_QUEUE_FILL_PERIOD_MS,
                          20U);
}
