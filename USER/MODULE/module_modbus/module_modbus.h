#ifndef MODULE_MODBUS_H
#define MODULE_MODBUS_H

#include <stdint.h>

#include "tf_board_config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODULE_MODBUS_RX_BUF_SIZE
#define MODULE_MODBUS_RX_BUF_SIZE 256U
#endif

#ifndef MODULE_MODBUS_RINGBUF_SIZE
#define MODULE_MODBUS_RINGBUF_SIZE 512U
#endif

#ifndef MODULE_MODBUS_TX_BUF_SIZE
#define MODULE_MODBUS_TX_BUF_SIZE 256U
#endif

#ifndef MODULE_MODBUS_SEND_BUF_SIZE
#define MODULE_MODBUS_SEND_BUF_SIZE 256U
#endif

#ifndef MODULE_MODBUS_SEND_QUEUE_MAX
#define MODULE_MODBUS_SEND_QUEUE_MAX 16U
#endif

#ifndef MODULE_MODBUS_MASTER_PERIOD_MS
#define MODULE_MODBUS_MASTER_PERIOD_MS 10U
#endif

#ifndef MODULE_MODBUS_QUEUE_FILL_PERIOD_MS
#define MODULE_MODBUS_QUEUE_FILL_PERIOD_MS 100U
#endif

typedef void (*module_modbus_rx_callback_t)(const uint8_t *data, uint32_t len);

typedef enum {
    MODULE_MODBUS_DEV_XM7828_TH = 0,
    MODULE_MODBUS_DEV_XM8189_WIND,
    MODULE_MODBUS_DEV_XM8537_WATER,
    MODULE_MODBUS_DEV_XM9505_RAIN,
    MODULE_MODBUS_DEV_AC,
    MODULE_MODBUS_DEV_CUSTOM,
} module_modbus_device_type_t;

typedef enum {
    MODULE_MODBUS_POLL_READ_REGS = 0,
    MODULE_MODBUS_POLL_READ_BITS,
} module_modbus_poll_function_t;

typedef enum {
    MODULE_MODBUS_MASTER_IDLE = 0,
    MODULE_MODBUS_MASTER_WAIT_RESPONSE,
} module_modbus_master_state_t;

typedef struct {
    module_modbus_master_state_t state;
    bool tx_busy;
    bool waiting_response;
    uint8_t waiting_addr;
    module_modbus_poll_function_t waiting_function;
    uint8_t poll_index;
    uint32_t tick_count;
    uint32_t request_count;
    uint32_t response_count;
    uint32_t timeout_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    uint32_t unmatched_count;
    uint32_t parse_ok_count;
    uint32_t parse_error_count;
    uint8_t queue_count;
    uint16_t send_buf_write_index;
    uint32_t enqueue_count;
    uint32_t dequeue_count;
    uint32_t queue_overwrite_drop_count;
    uint32_t queue_full_drop_count;
    uint32_t queue_invalid_count;
} module_modbus_task_status_t;

bool module_modbus_init(void);
void module_modbus_master_task(void);
void module_modbus_queue_fill_task(void);
bool module_modbus_send(const uint8_t *data, uint32_t len);
void module_modbus_set_rx_callback(module_modbus_rx_callback_t callback);
void module_modbus_get_task_status(module_modbus_task_status_t *status);
uint32_t module_modbus_available(void);
uint32_t module_modbus_read(uint8_t *buf, uint32_t buf_size);
uint32_t module_modbus_drop(uint32_t len);
uint32_t module_modbus_rx_overflow_count(void);
void module_modbus_usart2_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_H */
