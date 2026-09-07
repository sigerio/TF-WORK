#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "board_config.h"
#include "comm_message.h"
#include "comm_dispatch.h"
#include "comm_receive.h"
#include "task.h"

#define MAIN_QUEUE_ITEMS 20U
QueueHandle_t main_queue;



void vApplicationTickHook(void )
{

}


void vApplicationMallocFailedHook( void )
{

}


void vApplicationIdleHook (void)
{

}

void vApplicationDaemonTaskStartupHook(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_printf(0, "RTT ready\r\n");
    main_queue = xQueueCreate(MAIN_QUEUE_ITEMS, sizeof(my_message_node_t));
    configASSERT(main_queue != NULL);
    rx_frame_init();
    board_init();
    SEGGER_RTT_printf(0, "Board ready\r\n");
}
