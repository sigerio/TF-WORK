#ifndef MODBUS_TX_QUEUE_H
#define MODBUS_TX_QUEUE_H

#include "module_modbus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t addr;
    module_modbus_device_type_t device;
    module_modbus_poll_function_t function;
    uint16_t start;
    uint16_t count;
    uint8_t expected_func;
    uint8_t expected_bytes;
    uint16_t expected_len;
} modbus_response_expect_t;

typedef struct {
    uint16_t buf_index;
    uint16_t len;
    modbus_response_expect_t response;
} modbus_tx_queue_item_t;

typedef struct {
    uint8_t storage[MODULE_MODBUS_SEND_BUF_SIZE];
    modbus_tx_queue_item_t queue[MODULE_MODBUS_SEND_QUEUE_MAX];
    uint16_t write_index;
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t enqueue_count;
    uint32_t dequeue_count;
    uint32_t overwrite_drop_count;
    uint32_t full_drop_count;
    uint32_t invalid_count;
} modbus_tx_queue_t;

void modbus_tx_queue_reset(modbus_tx_queue_t *queue);
bool modbus_tx_queue_push(modbus_tx_queue_t *queue,
                          const uint8_t *frame,
                          uint16_t len,
                          const modbus_response_expect_t *response);
bool modbus_tx_queue_peek(const modbus_tx_queue_t *queue,
                          modbus_tx_queue_item_t *item);
void modbus_tx_queue_pop(modbus_tx_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TX_QUEUE_H */
