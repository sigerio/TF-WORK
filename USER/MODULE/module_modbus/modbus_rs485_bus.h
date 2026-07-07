#ifndef MODBUS_RS485_BUS_H
#define MODBUS_RS485_BUS_H

#include "module_modbus.h"
#include "ringbuf.h"
#include "tf_drv_dma.h"
#include "tf_drv_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    tf_drv_uart_handle_t uart;
    tf_drv_dma_handle_t dma;
} modbus_rs485_hw_t;

typedef struct {
    uint8_t dma_buf[MODULE_MODBUS_RX_BUF_SIZE];
    uint8_t ring_storage[MODULE_MODBUS_RINGBUF_SIZE];
    ringbuf_t ring;
    volatile uint32_t drop_count;
    module_modbus_rx_callback_t callback;
} modbus_rs485_rx_t;

typedef struct {
    uint8_t dma_buf[MODULE_MODBUS_TX_BUF_SIZE];
    volatile bool busy;
} modbus_rs485_tx_t;

typedef struct {
    modbus_rs485_hw_t hw;
    modbus_rs485_rx_t rx;
    modbus_rs485_tx_t tx;
} modbus_rs485_bus_t;

bool modbus_rs485_init(modbus_rs485_bus_t *bus);
void modbus_rs485_reset_runtime(modbus_rs485_bus_t *bus);

void modbus_rs485_set_rx_callback(modbus_rs485_bus_t *bus,
                                  module_modbus_rx_callback_t callback);
module_modbus_rx_callback_t modbus_rs485_get_rx_callback(const modbus_rs485_bus_t *bus);

bool modbus_rs485_send(modbus_rs485_bus_t *bus, const uint8_t *data, uint32_t len);
bool modbus_rs485_start_tx_dma(modbus_rs485_bus_t *bus, uint32_t len);
void modbus_rs485_tx_service(modbus_rs485_bus_t *bus);
bool modbus_rs485_tx_busy(const modbus_rs485_bus_t *bus);
uint8_t *modbus_rs485_tx_buffer(modbus_rs485_bus_t *bus);

uint32_t modbus_rs485_rx_available(modbus_rs485_bus_t *bus);
uint32_t modbus_rs485_rx_read(modbus_rs485_bus_t *bus, uint8_t *buf, uint32_t buf_size);
uint32_t modbus_rs485_rx_peek(modbus_rs485_bus_t *bus, uint8_t *buf, uint32_t len);
uint32_t modbus_rs485_rx_drop(modbus_rs485_bus_t *bus, uint32_t len);
uint32_t modbus_rs485_rx_overflow_count(modbus_rs485_bus_t *bus);
void modbus_rs485_rx_clear(modbus_rs485_bus_t *bus);

void modbus_rs485_usart_irq_handler(modbus_rs485_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RS485_BUS_H */
