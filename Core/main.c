#include "../USER/CTRL/KEY/key.h"
#include "../USER/CTRL/MOTO/module_moto.h"
#include "../USER/CTRL/MOTO/module_moto_sync.h"
#include "../USER/COMM/iec104/iec104_server.h"
#include "../USER/COMM/iec104/port/iec104_port_usart1.h"
#include "../USER/COMM/modbus/tf_modbus_master.h"
#include "../USER/CTRL/module_ws2812/module_ws2812.h"
#include "../USER/TASK/tf_usr_event_task.h"
#include "../USER/TASK/tf_usr_time_task.h"
#include "../USER/debug_console.h"
#include "../USER/tf_delay.h"
#include "../USER/OS/tf_os.h"

int main(void)
{
    tf_time_init();
    debug_console_init();
    (void)iec104_port_usart1_init();
    module_ws2812_init();
    tf_modbus_master_init();
    tf_key_init();
    module_moto_init();
    module_moto_sync_init();
    iec104_server_init();

    tf_os_init();
    tf_usr_time_task_init();
    tf_usr_event_task_init();

    tf_os_main_loop();
}
