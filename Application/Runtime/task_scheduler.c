#include "task_scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "comm_dispatch.h"
#include "comm_receive.h"
#include "board_config.h"

#define START_TASK_PRIORITY (configMAX_PRIORITIES - 2U)
#define START_TASK_STACK_SIZE 512U
#define COMM_RX_TASK_PRIORITY 6U
#define COMM_QUEUE_TASK_PRIORITY 5U
#define COMM_TASK_STACK_SIZE 512U
#define LED_TASK_PRIORITY 1U
#define LED_TASK_STACK_SIZE 64U

void run_led_task(void *pvParameters)
{
    bool led_on = false;
    (void)pvParameters;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        led_on = !led_on;
        gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_LED), BOARD_PIN_PIN(BOARD_PIN_LED),
                       led_on ? RESET : SET);
    }
}

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
    result = xTaskCreate(run_led_task, "run_led", LED_TASK_STACK_SIZE,
                         NULL, LED_TASK_PRIORITY, NULL);
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
