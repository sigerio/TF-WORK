#ifndef COMM_DISPATCH_H
#define COMM_DISPATCH_H

#include "FreeRTOS.h"
#include "comm_message.h"
#include "queue.h"

#define UART3_DEVICE_RAIN  0U
#define UART3_DEVICE_WATER 1U

#ifndef UART3_DEVICE
#define UART3_DEVICE UART3_DEVICE_WATER
#endif

#if UART3_DEVICE == UART3_DEVICE_RAIN
#define UART3_TX_MARK DATA_TO_RAIN
#define UART3_RX_MARK DATA_FROM_RAIN
#define UART3_MASTER_ADDR 3U
#elif UART3_DEVICE == UART3_DEVICE_WATER
#define UART3_TX_MARK DATA_TO_WATER
#define UART3_RX_MARK DATA_FROM_WATER
#define UART3_MASTER_ADDR 4U
#else
#error UART3_DEVICE must select UART3_DEVICE_RAIN or UART3_DEVICE_WATER
#endif

extern QueueHandle_t main_queue;

/* B4/41 notification: bit layout matches xj_jc APP/app_fault.h; do not reorder. */
typedef union {
    struct {
        /* P1: bits 0..3. */
        uint32_t emergency_stop : 1;
        uint32_t door_limit_conflict : 1;
        uint32_t platform_limit_conflict : 1;
        uint32_t water_detected : 1;
        /* P2: bits 4..11. */
        uint32_t door_start_failed : 1;
        uint32_t platform_start_failed : 1;
        uint32_t ac_high_temp : 1;
        uint32_t ac_low_temp : 1;
        uint32_t ac_humidity_high : 1;
        uint32_t ac_condenser_high_pressure : 1;
        uint32_t ac_evaporator_freeze : 1;
        uint32_t ac_cooling_alarm : 1;
        /* P3: bits 12..24. */
        uint32_t th_offline : 1;
        uint32_t wind_offline : 1;
        uint32_t water_offline : 1;
        uint32_t rain_offline : 1;
        uint32_t ac_offline : 1;
        uint32_t th_sensor_fault : 1;
        uint32_t wind_sensor_fault : 1;
        uint32_t water_sensor_fault : 1;
        uint32_t rain_sensor_fault : 1;
        uint32_t ac_cabinet_sensor_fault : 1;
        uint32_t ac_evaporator_sensor_fault : 1;
        uint32_t ac_condenser_sensor_fault : 1;
        uint32_t ac_humidity_sensor_fault : 1;
        uint32_t reserved : 7;
    } bits;
    struct {
        uint32_t p1 : 4;
        uint32_t p2 : 8;
        uint32_t p3 : 13;
        uint32_t reserved : 7;
    } levels;
    uint32_t raw;
} weather_fault_u;

/* Task-context snapshot. False for NULL/before the first valid notification;
   output unchanged on false. True means received, not a live/timeout check.
   water_detected=0 is valid dry only if water_offline=water_sensor_fault=0. */
bool weather_fault_get(weather_fault_u *fault);
void comm_log_frame(DATA_MARK_E data_mark, const uint8_t *data, uint16_t data_len);
bool message_send(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len);
bool node_analyze(my_message_node_t node);
void queue_pack_read(void *pvParameters);

#endif
