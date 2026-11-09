#ifndef COMM_DISPATCH_H
#define COMM_DISPATCH_H

#include "FreeRTOS.h"
#include "comm_message.h"
#include "queue.h"

#define MODBUS_ADDR_MACHINE 0x11U
#define MODBUS_ADDR_POLE    0x12U
#define MODBUS_ADDR_SOLAR   0x13U
#define MODBUS_ADDR_MT80     0x14U
#define MODBUS_ADDR_SINGLE_SLAVE 0x01U
#define MODBUS_ADDR_MT80_SLAVE   0x0AU

extern QueueHandle_t main_queue;

bool comm_address_allowed(DATA_MARK_E source, uint8_t address);
void comm_log_frame(DATA_MARK_E data_mark, const uint8_t *data, uint16_t data_len);
bool message_send(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len);
bool node_analyze(my_message_node_t node);
void queue_pack_read(void *pvParameters);

#endif
