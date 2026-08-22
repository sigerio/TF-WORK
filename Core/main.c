#include "../USER/CTRL/KEY/key.h"
#include "../USER/CTRL/MOTO/motor_driver.h"
#include "../USER/CTRL/MOTO/motor_motion.h"
#include "../USER/COMM/iec104/iec104_server.h"
#include "../USER/COMM/iec104/port/iec104_port.h"
#include "../USER/COMM/modbus/tf_modbus_master.h"
#include "../USER/CTRL/LIGHT/light_output.h"
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
    (void)iec104_port_init();
    module_ws2812_init();
    light_output_init();
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
