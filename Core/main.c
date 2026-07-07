#include "../USER/uart_and_dma.h"
#include "../USER/MODULE/module_modbus/module_modbus.h"
#include "../TFSDK/framework/os/tf_os.h"

static void module_modbus_master_timer_task(void *arg)
{
    (void)arg;
    module_modbus_master_task();
}

static void module_modbus_queue_fill_timer_task(void *arg)
{
    (void)arg;
    module_modbus_queue_fill_task();
}

int main(void)
{
    use_uart_and_dma();
    module_modbus_init();

    tf_os_init();
    tf_os_soft_timer_task(module_modbus_master_timer_task,
                          0,
                          MODULE_MODBUS_MASTER_PERIOD_MS,
                          10U);
    tf_os_soft_timer_task(module_modbus_queue_fill_timer_task,
                          0,
                          MODULE_MODBUS_QUEUE_FILL_PERIOD_MS,
                          20U);
    tf_os_main_loop();

    return 0;
}
