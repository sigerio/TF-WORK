#include "task_scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "comm_dispatch.h"
#include "comm_receive.h"

#define START_TASK_PRIORITY (configMAX_PRIORITIES - 2U)
#define START_TASK_STACK_SIZE 512U
#define COMM_RX_TASK_PRIORITY 6U
#define COMM_QUEUE_TASK_PRIORITY 5U
#define COMM_TASK_STACK_SIZE 512U

static void start_task(void *parameters)
{
    BaseType_t result;

    (void)parameters;
    result = xTaskCreate(communication_rx_task, "comm_rx", COMM_TASK_STACK_SIZE,
                         NULL, COMM_RX_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(queue_pack_read, "comm_queue", COMM_TASK_STACK_SIZE,
                         NULL, COMM_QUEUE_TASK_PRIORITY, NULL);
    configASSERT(result == pdPASS);
    vTaskDelete(NULL);
}

void task_scheduler(void)
{
    BaseType_t result = xTaskCreate(start_task, "start_task", START_TASK_STACK_SIZE,
                                    NULL, START_TASK_PRIORITY, NULL);

    configASSERT(result == pdPASS);
    vTaskStartScheduler();
    for (;;) {
    }
}
