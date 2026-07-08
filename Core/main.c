#include "../USER/uart_and_dma.h"
#include "../USER/MODULE/module_modbus/tf_485_master.h"
#include "../TFSDK/framework/os/tf_os.h"

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

int main(void)
{
    use_uart_and_dma();
    tf_485_master_init();

    tf_os_init();
    tf_os_soft_timer_task(tf_485_master_timer_task,
                          0,
                          MODULE_MODBUS_MASTER_PERIOD_MS,
                          10U);
    tf_os_soft_timer_task(tf_485_query_timer_task,
                          0,
                          MODULE_MODBUS_QUEUE_FILL_PERIOD_MS,
                          20U);
    tf_os_main_loop();

    return 0;
}
