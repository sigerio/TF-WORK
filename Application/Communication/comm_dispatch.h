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
bool message_send(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len);
bool node_analyze(my_message_node_t node);
void queue_pack_read(void *pvParameters);

#endif
