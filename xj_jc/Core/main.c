#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"
#include "COMM/iec104/iec104_server.h"
#include "COMM/iec104/port/iec104_port.h"
#include "COMM/modbus/modbus_master.h"
#include "CONTROL/output/aux_output.h"
#include "CONTROL/ws2812/driver/ws2812_driver.h"
#include "APP/tasks/app_event_task.h"
#include "APP/tasks/app_timer_task.h"
#include "DEBUG/debug_console.h"
#include "BOARD/board_time.h"
#include "OS/os.h"

int main(void)
{
    board_time_init();
    debug_console_init();
    (void)iec104_port_init();
    ws2812_driver_init();
    aux_output_init();
    modbus_master_init();
    key_init();
    motor_control_init();
    iec104_server_init();

    os_init();
    usr_time_task_init();
    usr_event_task_init();

    os_main_loop();
}
