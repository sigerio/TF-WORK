#include "COMM/modbus/modbus_port.h"

#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"

typedef struct {
    com_link_resource_t resource;
    uint8_t rx_dma_buf[MODULE_MODBUS_RX_BUF_SIZE];
    uint8_t rx_fifo_buf[MODULE_MODBUS_RINGBUF_SIZE];
    ringbuf_t rx_fifo;
    uint8_t tx_dma_buf[MODULE_MODBUS_TX_BUF_SIZE];
} modbus_port_buffers_t;

static modbus_port_buffers_t bus_buffers[MODBUS_BUS_COUNT];

static void modbus_bus_hardware_init(const board_usart_config_t *usart)
{
    const board_gpio_config_t *tx_pin = &board_gpio_configs[usart->tx_pin];
    const board_gpio_config_t *rx_pin = &board_gpio_configs[usart->rx_pin];
    const board_dma_config_t *dma = &board_dma_configs[usart->rx_dma.dma];
    const board_dma_channel_config_t *rx_dma = &dma->ch[usart->rx_dma.ch];
    dma_parameter_struct dma_parameters;

    /* TX/RX/方向脚 GPIO */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_init(tx_pin->port, tx_pin->mode, tx_pin->speed, tx_pin->pin);
    gpio_bit_write(tx_pin->port, tx_pin->pin, tx_pin->initial_level ? SET : RESET);
    gpio_init(rx_pin->port, rx_pin->mode, rx_pin->speed, rx_pin->pin);

    if (usart->direction_pin != BOARD_PIN_NONE) {
        const board_gpio_config_t *dir = &board_gpio_configs[usart->direction_pin];

        gpio_init(dir->port, dir->mode, dir->speed, dir->pin);
        gpio_bit_write(dir->port, dir->pin, dir->initial_level ? SET : RESET);
    }

    /* USART */
    rcu_periph_clock_enable(usart->clock);
    usart_deinit(usart->peripheral);
    usart_baudrate_set(usart->peripheral, usart->baudrate);
    usart_word_length_set(usart->peripheral, USART_WL_8BIT);
    usart_stop_bit_set(usart->peripheral, USART_STB_1BIT);
    usart_parity_config(usart->peripheral, USART_PM_NONE);
    usart_hardware_flow_rts_config(usart->peripheral, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(usart->peripheral, USART_CTS_DISABLE);
    usart_receive_config(usart->peripheral, USART_RECEIVE_ENABLE);
    usart_transmit_config(usart->peripheral, USART_TRANSMIT_ENABLE);
    usart_enable(usart->peripheral);

    /* RX DMA */
    rcu_periph_clock_enable(dma->clock);
    dma_struct_para_init(&dma_parameters);
    dma_parameters.memory_addr = 0U;
    dma_parameters.number = 0U;
    dma_parameters.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_parameters.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_parameters.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_parameters.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_parameters.priority = DMA_PRIORITY_ULTRA_HIGH;

    dma_parameters.periph_addr = rx_dma->peripheral_address;
    dma_parameters.direction = rx_dma->direction;
    dma_deinit(dma->controller, rx_dma->channel);
    dma_init(dma->controller, rx_dma->channel, &dma_parameters);
    dma_circulation_disable(dma->controller, rx_dma->channel);
    dma_memory_to_memory_disable(dma->controller, rx_dma->channel);
    dma_flag_clear(dma->controller, rx_dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);

    /* TX DMA（可选：无 DMA 通道时跳过，TX 走轮询） */
    if (BOARD_DMA_HAS(usart->tx_dma)) {
        const board_dma_config_t *tx_dma_ctl = &board_dma_configs[usart->tx_dma.dma];
        const board_dma_channel_config_t *tx_dma = &tx_dma_ctl->ch[usart->tx_dma.ch];

        rcu_periph_clock_enable(tx_dma_ctl->clock);
        dma_parameters.periph_addr = tx_dma->peripheral_address;
        dma_parameters.direction = tx_dma->direction;
        dma_deinit(tx_dma_ctl->controller, tx_dma->channel);
        dma_init(tx_dma_ctl->controller, tx_dma->channel, &dma_parameters);
        dma_circulation_disable(tx_dma_ctl->controller, tx_dma->channel);
        dma_memory_to_memory_disable(tx_dma_ctl->controller, tx_dma->channel);
        dma_flag_clear(tx_dma_ctl->controller, tx_dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
        usart_dma_transmit_config(usart->peripheral, USART_TRANSMIT_DMA_ENABLE);
    }

    usart_dma_receive_config(usart->peripheral, USART_RECEIVE_DMA_ENABLE);
}

bool modbus_port_open(modbus_bus_t bus, com_link_t *link)
{
    modbus_port_buffers_t *buffers;
    com_link_resource_t *resource;

    if (((uint32_t)bus >= MODBUS_BUS_COUNT) || (link == NULL)) {
        return false;
    }
    buffers = &bus_buffers[bus];
    resource = &buffers->resource;
    resource->usart = &board_usart_configs[(bus == MODBUS_BUS_WEATHER) ? BOARD_USART_WEATHER : BOARD_USART_AC];
    resource->rx_dma_buf = buffers->rx_dma_buf;
    resource->rx_dma_size = (uint16_t)sizeof(buffers->rx_dma_buf);
    resource->rx_fifo = &buffers->rx_fifo;
    resource->tx_dma_buf = buffers->tx_dma_buf;
    resource->tx_dma_size = (uint16_t)sizeof(buffers->tx_dma_buf);

    modbus_bus_hardware_init(resource->usart);
    nvic_irq_enable(com_link_usart_irqn(resource->usart->peripheral), 1U, 0U);
    ringbuf_init(&buffers->rx_fifo, buffers->rx_fifo_buf, sizeof(buffers->rx_fifo_buf));
    return com_link_open(link, resource);
}
