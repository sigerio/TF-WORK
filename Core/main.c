#include "../USER/uart_and_dma.h"
#include "../USER/MODULE/KEY/key.h"
#include "../USER/MODULE/MOTO/module_moto.h"
#include "../USER/MODULE/MOTO/module_moto_hall.h"
#include "../USER/MODULE/module_modbus/tf_485_master.h"
#include "../USER/TASK/tf_usr_event_task.h"
#include "../USER/TASK/tf_usr_time_task.h"
#include "../TFSDK/framework/os/tf_os.h"

int main(void)
{
    use_uart_and_dma();
    tf_485_master_init();
    tf_key_init();
    module_moto_init();
#if MODULE_MOTO_HALL_ENABLE
    module_moto_hall_init();
#endif

    tf_os_init();
    tf_usr_time_task_init();
    tf_usr_event_task_init();

    tf_os_main_loop();

    return 0;
}
