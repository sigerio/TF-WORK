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

/* Same display priority as xj_jc ws2812_app.c. */
typedef enum {
    BUZZER_ALARM_NONE = 0,
    BUZZER_ALARM_WATER,
    BUZZER_ALARM_LIMIT,
    BUZZER_ALARM_ESTOP
} buzzer_alarm_e;

void run_led_task(void *pvParameters)
{
    uint8_t run_cnt = 0U;
    bool led_on = false;
    bool beep_on = false;
    bool water_alarm_held = false;
    buzzer_alarm_e alarm = BUZZER_ALARM_NONE;
    buzzer_alarm_e pending_alarm = BUZZER_ALARM_NONE;
    TickType_t alarm_started = 0U;
    TickType_t recovery_started = 0U;
    weather_fault_u fault;
    (void)pvParameters;
    for (;;)
    {
        if(run_cnt == 50)
        {
            led_on = !led_on;
            gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_LED), BOARD_PIN_PIN(BOARD_PIN_LED),
                           led_on ? RESET : SET);
            run_cnt = 0;
        }
        run_cnt++;

        beep_on = false;
        if (weather_fault_get(&fault)) {
            TickType_t now = xTaskGetTickCount();
            buzzer_alarm_e requested = BUZZER_ALARM_NONE;

            if (fault.bits.water_detected) {
                water_alarm_held = true;
            } else if (!fault.bits.water_offline && !fault.bits.water_sensor_fault) {
                water_alarm_held = false;
            }
            if (fault.bits.emergency_stop) {
                requested = BUZZER_ALARM_ESTOP;
            } else if (fault.bits.door_limit_conflict || fault.bits.platform_limit_conflict) {
                requested = BUZZER_ALARM_LIMIT;
            } else if (water_alarm_held) {
                requested = BUZZER_ALARM_WATER;
            }

            /* Upgrade immediately; downgrade/recover only after 2 s of stability. */
            if (requested >= alarm) {
                pending_alarm = requested;
                recovery_started = now;
                if (requested != alarm) {
                    alarm = requested;
                    alarm_started = now;
                }
            } else {
                if (requested != pending_alarm) {
                    pending_alarm = requested;
                    recovery_started = now;
                }
                if ((TickType_t)(now - recovery_started) >= pdMS_TO_TICKS(2000U)) {
                    alarm = requested;
                    alarm_started = now;
                }
            }

            /* B4 carries no light phase: start the matching rhythm on local receipt. */
            if (alarm == BUZZER_ALARM_LIMIT) {
                TickType_t phase = (TickType_t)(now - alarm_started) % pdMS_TO_TICKS(1500U);
                beep_on = phase < pdMS_TO_TICKS(150U) ||
                          (phase >= pdMS_TO_TICKS(300U) && phase < pdMS_TO_TICKS(450U));
            } else if (alarm == BUZZER_ALARM_WATER) {
                beep_on = (TickType_t)(now - alarm_started) % pdMS_TO_TICKS(1000U) < pdMS_TO_TICKS(500U);
            }
            /* ESTOP is the master's highest-priority light effect, but stays silent. */
        }
        gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_BEEP), BOARD_PIN_PIN(BOARD_PIN_BEEP), beep_on ? SET : RESET);
        vTaskDelay(pdMS_TO_TICKS(10));
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
