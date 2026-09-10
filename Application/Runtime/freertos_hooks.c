#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "board_config.h"
#include "comm_message.h"
#include "comm_dispatch.h"
#include "comm_receive.h"
#include "task.h"
#include "gd32f10x_fwdgt.h"

#define MAIN_QUEUE_ITEMS 20U
/* IRC40K / 64: (1249 + 1) ticks gives a nominal 2 s reset timeout. */
#define WATCHDOG_RELOAD_VALUE 1249U
QueueHandle_t main_queue;
static BaseType_t watchdog_ready = pdFALSE;


void vApplicationTickHook(void )
{

}


void vApplicationMallocFailedHook( void )
{

}


void vApplicationIdleHook (void)
{
    if (watchdog_ready != pdFALSE) {
        fwdgt_counter_reload();
    }
}

void vApplicationDaemonTaskStartupHook(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_SetFlagsUpBuffer(0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    SEGGER_RTT_printf(0, "RTT ready\r\n");
    main_queue = xQueueCreate(MAIN_QUEUE_ITEMS, sizeof(my_message_node_t));
    configASSERT(main_queue != NULL);
    rx_frame_init();
    board_init();
    SEGGER_RTT_printf(0, "Board ready\r\n");
    /* fwdgt_config() starts and reloads the watchdog on success. */
    if (fwdgt_config(WATCHDOG_RELOAD_VALUE, FWDGT_PSC_DIV64) != SUCCESS) {
        configASSERT(0);
        return;
    }
    watchdog_ready = pdTRUE;
}
