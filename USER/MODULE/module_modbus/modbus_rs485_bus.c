#include "modbus_rs485_bus.h"

#include <string.h>

#include "gd32f10x_misc.h"
#include "tf_board_dma.h"
#include "tf_board_uart.h"
#include "tf_hal_dma.h"
#include "tf_hal_uart.h"

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#define MODBUS_UART_ID      E_DRV_UART_2
#define MODBUS_DMA_ID       E_DRV_DMA_1
#define MODBUS_RX_DMA_CH    E_DRV_D0_CH2
#define MODBUS_TX_DMA_CH    E_DRV_D0_CH4

static void *modbus_rs485_uart_base(void)
{
    return uart_configs[MODBUS_UART_ID].base;
}

static void *modbus_rs485_dma_base(void)
{
    return dma_configs[MODBUS_DMA_ID].base;
}

static uint8_t modbus_rs485_rx_hw_channel(void)
{
    return (uint8_t)dma_configs[MODBUS_DMA_ID].ch[MODBUS_RX_DMA_CH].channel;
}

static uint8_t modbus_rs485_tx_hw_channel(void)
{
    return (uint8_t)dma_configs[MODBUS_DMA_ID].ch[MODBUS_TX_DMA_CH].channel;
}

static void modbus_rs485_rx_stop(modbus_rs485_bus_t *bus)
{
    tf_hal_config_uart_dma_ch(modbus_rs485_uart_base(),
                              modbus_rs485_rx_hw_channel(),
                              0U);
    tf_drv_dma_stop(bus->hw.dma, MODBUS_RX_DMA_CH);
}

static void modbus_rs485_rx_restart(modbus_rs485_bus_t *bus)
{
    modbus_rs485_rx_stop(bus);

    tf_drv_dma_addr_config(bus->hw.dma,
                           MODBUS_RX_DMA_CH,
                           dma_configs[MODBUS_DMA_ID].ch[MODBUS_RX_DMA_CH].periph_addr,
                           (uint32_t)bus->rx.dma_buf);
    tf_drv_dma_number_config(bus->hw.dma, MODBUS_RX_DMA_CH, MODULE_MODBUS_RX_BUF_SIZE);
    tf_hal_dma_flag_clear(modbus_rs485_dma_base(),
                          modbus_rs485_rx_hw_channel(),
                          DMA_FLAG_G);
    tf_hal_uart_rx_flush(modbus_rs485_uart_base());
    tf_hal_config_uart_dma_ch(modbus_rs485_uart_base(),
                              modbus_rs485_rx_hw_channel(),
                              1U);
    tf_drv_dma_start(bus->hw.dma, MODBUS_RX_DMA_CH);
}

bool modbus_rs485_init(modbus_rs485_bus_t *bus)
{
    if (bus == NULL) {
        return false;
    }

    memset(bus, 0, sizeof(*bus));
    bus->hw.uart = tf_drv_uart_register(MODBUS_UART_ID);
    bus->hw.dma = tf_drv_dma_register(MODBUS_DMA_ID);

    if ((bus->hw.uart == NULL) || (bus->hw.dma == NULL)) {
        return false;
    }

    ringbuf_init(&bus->rx.ring, bus->rx.ring_storage, sizeof(bus->rx.ring_storage));
    modbus_rs485_rx_restart(bus);
    tf_hal_uart_interrupt_enable(modbus_rs485_uart_base(), USART_INT_IDLE);
    nvic_irq_enable(USART2_IRQn, 1U, 0U);

    return true;
}

void modbus_rs485_reset_runtime(modbus_rs485_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }

    bus->rx.drop_count = 0U;
    bus->tx.busy = false;
    modbus_rs485_rx_clear(bus);
}

void modbus_rs485_set_rx_callback(modbus_rs485_bus_t *bus,
                                  module_modbus_rx_callback_t callback)
{
    if (bus == NULL) {
        return;
    }

    bus->rx.callback = callback;
}

module_modbus_rx_callback_t modbus_rs485_get_rx_callback(const modbus_rs485_bus_t *bus)
{
    if (bus == NULL) {
        return NULL;
    }

    return bus->rx.callback;
}

bool modbus_rs485_tx_busy(const modbus_rs485_bus_t *bus)
{
    return (bus != NULL) ? bus->tx.busy : false;
}

uint8_t *modbus_rs485_tx_buffer(modbus_rs485_bus_t *bus)
{
    return (bus != NULL) ? bus->tx.dma_buf : NULL;
}

void modbus_rs485_tx_service(modbus_rs485_bus_t *bus)
{
    if ((bus == NULL) || !bus->tx.busy) {
        return;
    }

    if ((tf_hal_dma_flag_get(modbus_rs485_dma_base(),
                             modbus_rs485_tx_hw_channel(),
                             DMA_FLAG_FTF) == 0U) ||
        (tf_hal_uart_flag_get(modbus_rs485_uart_base(), USART_FLAG_TC) == 0U)) {
        return;
    }

    tf_hal_config_uart_dma_ch(modbus_rs485_uart_base(),
                              modbus_rs485_tx_hw_channel(),
                              0U);
    tf_drv_dma_stop(bus->hw.dma, MODBUS_TX_DMA_CH);
    tf_hal_dma_flag_clear(modbus_rs485_dma_base(),
                          modbus_rs485_tx_hw_channel(),
                          DMA_FLAG_G);
    bus->tx.busy = false;
}

bool modbus_rs485_start_tx_dma(modbus_rs485_bus_t *bus, uint32_t len)
{
    if ((bus == NULL) || (bus->hw.uart == NULL) || (bus->hw.dma == NULL) ||
        (len == 0U) || (len > MODULE_MODBUS_TX_BUF_SIZE) || bus->tx.busy) {
        return false;
    }

    tf_hal_config_uart_dma_ch(modbus_rs485_uart_base(),
                              modbus_rs485_tx_hw_channel(),
                              0U);
    tf_drv_dma_stop(bus->hw.dma, MODBUS_TX_DMA_CH);
    tf_drv_dma_addr_config(bus->hw.dma,
                           MODBUS_TX_DMA_CH,
                           dma_configs[MODBUS_DMA_ID].ch[MODBUS_TX_DMA_CH].periph_addr,
                           (uint32_t)bus->tx.dma_buf);
    tf_drv_dma_number_config(bus->hw.dma, MODBUS_TX_DMA_CH, len);
    tf_hal_dma_flag_clear(modbus_rs485_dma_base(),
                          modbus_rs485_tx_hw_channel(),
                          DMA_FLAG_G);
    tf_hal_uart_flag_clear(modbus_rs485_uart_base(), USART_FLAG_TC);
    tf_hal_config_uart_dma_ch(modbus_rs485_uart_base(),
                              modbus_rs485_tx_hw_channel(),
                              1U);
    tf_drv_dma_start(bus->hw.dma, MODBUS_TX_DMA_CH);
    bus->tx.busy = true;

    return true;
}

bool modbus_rs485_send(modbus_rs485_bus_t *bus, const uint8_t *data, uint32_t len)
{
    if ((bus == NULL) || (data == NULL) || (len == 0U) ||
        (len > MODULE_MODBUS_TX_BUF_SIZE) || bus->tx.busy) {
        return false;
    }

    memcpy(bus->tx.dma_buf, data, len);
    return modbus_rs485_start_tx_dma(bus, len);
}

void modbus_rs485_rx_clear(modbus_rs485_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }

    __disable_irq();
    ringbuf_reset(&bus->rx.ring);
    __enable_irq();
}

uint32_t modbus_rs485_rx_available(modbus_rs485_bus_t *bus)
{
    uint32_t available;

    if (bus == NULL) {
        return 0U;
    }

    __disable_irq();
    available = ringbuf_available(&bus->rx.ring);
    __enable_irq();

    return available;
}

uint32_t modbus_rs485_rx_read(modbus_rs485_bus_t *bus,
                              uint8_t *buf,
                              uint32_t buf_size)
{
    uint32_t len;

    if ((bus == NULL) || (buf == NULL) || (buf_size == 0U)) {
        return 0U;
    }

    __disable_irq();
    len = ringbuf_read(&bus->rx.ring, buf, buf_size);
    __enable_irq();

    return len;
}

uint32_t modbus_rs485_rx_peek(modbus_rs485_bus_t *bus,
                              uint8_t *buf,
                              uint32_t len)
{
    uint32_t peek_len;

    if ((bus == NULL) || (buf == NULL) || (len == 0U)) {
        return 0U;
    }

    __disable_irq();
    peek_len = ringbuf_peek(&bus->rx.ring, buf, len);
    __enable_irq();

    return peek_len;
}

uint32_t modbus_rs485_rx_drop(modbus_rs485_bus_t *bus, uint32_t len)
{
    uint32_t drop_len;

    if (bus == NULL) {
        return 0U;
    }

    __disable_irq();
    drop_len = ringbuf_drop(&bus->rx.ring, len);
    __enable_irq();

    return drop_len;
}

uint32_t modbus_rs485_rx_overflow_count(modbus_rs485_bus_t *bus)
{
    uint32_t overflow;

    if (bus == NULL) {
        return 0U;
    }

    __disable_irq();
    overflow = ringbuf_overflow_count(&bus->rx.ring) + bus->rx.drop_count;
    __enable_irq();

    return overflow;
}

void modbus_rs485_usart_irq_handler(modbus_rs485_bus_t *bus)
{
    uint32_t remain;
    uint32_t rx_len;

    if ((bus == NULL) || (bus->hw.uart == NULL) || (bus->hw.dma == NULL)) {
        tf_hal_uart_idle_clear(modbus_rs485_uart_base());
        return;
    }

    if (tf_hal_uart_interrupt_flag_get(modbus_rs485_uart_base(),
                                       USART_INT_FLAG_IDLE) != 0U) {
        tf_hal_uart_idle_clear(modbus_rs485_uart_base());
        tf_drv_dma_stop(bus->hw.dma, MODBUS_RX_DMA_CH);

        remain = tf_drv_dma_remaining_get(bus->hw.dma, MODBUS_RX_DMA_CH);
        rx_len = MODULE_MODBUS_RX_BUF_SIZE - remain;

        if ((rx_len > 0U) && (rx_len <= MODULE_MODBUS_RX_BUF_SIZE)) {
            if (ringbuf_write(&bus->rx.ring, bus->rx.dma_buf, rx_len) < rx_len) {
                bus->rx.drop_count++;
            }
        }

        modbus_rs485_rx_restart(bus);
    }
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
