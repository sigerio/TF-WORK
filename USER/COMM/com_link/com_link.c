#include "com_link.h"

#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"

/* ── 由 board 配置解析出的硬件资源访问宏 ── */
#define LINK_USART(link)  ((link)->res->usart->peripheral)
#define LINK_RX_DMA_CTL(link) (board_dma_configs[(link)->res->usart->rx_dma.dma].controller)
#define LINK_RX_DMA_CH(link)  (board_dma_configs[(link)->res->usart->rx_dma.dma].ch[(link)->res->usart->rx_dma.ch].channel)
#define LINK_RX_DMA_ADDR(link) (board_dma_configs[(link)->res->usart->rx_dma.dma].ch[(link)->res->usart->rx_dma.ch].peripheral_address)
#define LINK_TX_DMA_CTL(link) (board_dma_configs[(link)->res->usart->tx_dma.dma].controller)
#define LINK_TX_DMA_CH(link)  (board_dma_configs[(link)->res->usart->tx_dma.dma].ch[(link)->res->usart->tx_dma.ch].channel)
#define LINK_TX_DMA_ADDR(link) (board_dma_configs[(link)->res->usart->tx_dma.dma].ch[(link)->res->usart->tx_dma.ch].peripheral_address)

static bool com_link_has_direction(const com_link_t *link)
{
    return (link->res->usart->direction_pin != BOARD_PIN_NONE);
}

/* HD 设备 DMA0 仅 7 通道，USART0_TX 无专用 DMA 通道 → 该链路发送退化为轮询 */
static bool com_link_has_tx_dma(const com_link_t *link)
{
    return BOARD_DMA_HAS((link)->res->usart->tx_dma);
}

/* 485 方向脚：initial_level=0 → 低=接收(默认)、高=发送 */
static void com_link_dir_rx(com_link_t *link)
{
    const board_gpio_config_t *dir =
        &board_gpio_configs[link->res->usart->direction_pin];

    gpio_bit_write(dir->port, dir->pin, dir->initial_level ? SET : RESET);
}

static void com_link_dir_tx(com_link_t *link)
{
    const board_gpio_config_t *dir =
        &board_gpio_configs[link->res->usart->direction_pin];

    gpio_bit_write(dir->port, dir->pin, dir->initial_level ? RESET : SET);
}

static void com_link_clear_idle_flag(com_link_t *link)
{
    volatile uint32_t stat = USART_STAT(LINK_USART(link));
    volatile uint32_t data = USART_DATA(LINK_USART(link));

    (void)stat;
    (void)data;
}

static void com_link_stop_rx_dma(com_link_t *link)
{
    dma_channel_disable(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link));
    usart_dma_receive_config(LINK_USART(link), USART_RECEIVE_DMA_DISABLE);
}

static void com_link_restart_rx_dma(com_link_t *link)
{
    com_link_stop_rx_dma(link);
    dma_periph_address_config(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link),
                              LINK_RX_DMA_ADDR(link));
    dma_memory_address_config(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link),
                              (uint32_t)link->res->rx_dma_buf);
    dma_transfer_number_config(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link),
                               link->res->rx_dma_size);
    dma_flag_clear(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link), DMA_FLAG_G);
    com_link_clear_idle_flag(link);
    while (usart_flag_get(LINK_USART(link), USART_FLAG_RBNE) != RESET) {
        (void)usart_data_receive(LINK_USART(link));
    }
    usart_dma_receive_config(LINK_USART(link), USART_RECEIVE_DMA_ENABLE);
    dma_channel_enable(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link));
}

/* 外设基址 → IRQn 查表工具（供使用方使能 NVIC） */
IRQn_Type com_link_usart_irqn(uint32_t usart_periph)
{
    if (usart_periph == USART0) { return USART0_IRQn; }
    if (usart_periph == USART1) { return USART1_IRQn; }
    if (usart_periph == USART2) { return USART2_IRQn; }
    if (usart_periph == UART3) { return UART3_IRQn; }
    if (usart_periph == UART4) { return UART4_IRQn; }
    return USART0_IRQn;
}

bool com_link_open(com_link_t *link, const com_link_resource_t *res)
{
    if ((link == NULL) || (res == NULL) || (res->usart == NULL) ||
        (res->rx_dma_buf == NULL) || (res->rx_fifo == NULL) ||
        (res->tx_dma_buf == NULL) || (res->rx_dma_size == 0U) ||
        (res->tx_dma_size == 0U)) {
        return false;
    }

    /* 只绑定运行时资源并启动接收链路；硬件(RCC/GPIO/USART/DMA/NVIC)
     * 由使用方在自己的 init 里用官方库函数完成。 */
    link->res = res;
    link->tx_busy = false;
    link->initialized = false;

    com_link_restart_rx_dma(link);
    usart_interrupt_enable(LINK_USART(link), USART_INT_IDLE);
    link->initialized = true;

    return true;
}

void com_link_reset(com_link_t *link)
{
    if (link == NULL) {
        return;
    }

    link->tx_busy = false;
    if (com_link_has_direction(link)) {
        com_link_dir_rx(link);
    }
    com_link_rx_clear(link);
}

uint8_t *com_link_tx_buffer(com_link_t *link)
{
    return ((link != NULL) && link->initialized) ? link->res->tx_dma_buf : NULL;
}

bool com_link_tx_busy(const com_link_t *link)
{
    return (link != NULL) ? link->tx_busy : false;
}

bool com_link_tx_start(com_link_t *link, uint16_t len)
{
    if ((link == NULL) || !link->initialized || (len == 0U) ||
        (len > link->res->tx_dma_size) || link->tx_busy) {
        return false;
    }

    if (com_link_has_direction(link)) {
        com_link_dir_tx(link);
    }

    if (!com_link_has_tx_dma(link)) {
        /*  */
        for (uint16_t i = 0U; i < len; i++) {
            while (usart_flag_get(LINK_USART(link), USART_FLAG_TBE) == RESET) {
            }
            usart_data_transmit(LINK_USART(link), link->res->tx_dma_buf[i]);
        }
        while (usart_flag_get(LINK_USART(link), USART_FLAG_TC) == RESET) {
        }
        usart_flag_clear(LINK_USART(link), USART_FLAG_TC);
        if (com_link_has_direction(link)) {
            com_link_dir_rx(link);
        }
        return true;   /* 发送已同步完成，不置 busy */
    }

    usart_dma_transmit_config(LINK_USART(link), USART_TRANSMIT_DMA_DISABLE);
    dma_channel_disable(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link));
    dma_periph_address_config(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link),
                              LINK_TX_DMA_ADDR(link));
    dma_memory_address_config(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link),
                              (uint32_t)link->res->tx_dma_buf);
    dma_transfer_number_config(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link), len);
    dma_flag_clear(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link), DMA_FLAG_G);
    usart_flag_clear(LINK_USART(link), USART_FLAG_TC);
    usart_dma_transmit_config(LINK_USART(link), USART_TRANSMIT_DMA_ENABLE);
    dma_channel_enable(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link));
    link->tx_busy = true;

    return true;
}

void com_link_tx_service(com_link_t *link)
{
    if ((link == NULL) || !link->tx_busy || !com_link_has_tx_dma(link)) {
        return;
    }

    if ((dma_flag_get(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link),
                      DMA_FLAG_FTF) == RESET) ||
        (usart_flag_get(LINK_USART(link), USART_FLAG_TC) == RESET)) {
        return;
    }

    usart_dma_transmit_config(LINK_USART(link), USART_TRANSMIT_DMA_DISABLE);
    dma_channel_disable(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link));
    dma_flag_clear(LINK_TX_DMA_CTL(link), LINK_TX_DMA_CH(link), DMA_FLAG_G);
    if (com_link_has_direction(link)) {
        com_link_dir_rx(link);
    }
    link->tx_busy = false;
}

uint16_t com_link_rx_available(com_link_t *link)
{
    uint32_t available;

    if ((link == NULL) || !link->initialized) {
        return 0U;
    }

    __disable_irq();
    available = ringbuf_available(link->res->rx_fifo);
    __enable_irq();

    return (uint16_t)available;
}

uint16_t com_link_rx_read(com_link_t *link, uint8_t *buf, uint16_t len)
{
    uint32_t read_len;

    if ((link == NULL) || (buf == NULL) || (len == 0U)) {
        return 0U;
    }

    __disable_irq();
    read_len = ringbuf_read(link->res->rx_fifo, buf, len);
    __enable_irq();

    return (uint16_t)read_len;
}

uint16_t com_link_rx_read_exact(com_link_t *link, uint8_t *buf, uint16_t len)
{
    uint32_t read_len;

    if ((link == NULL) || (buf == NULL) || (len == 0U)) {
        return 0U;
    }

    __disable_irq();
    read_len = ringbuf_read_exact(link->res->rx_fifo, buf, len);
    __enable_irq();

    return (uint16_t)read_len;
}

uint16_t com_link_rx_peek(com_link_t *link, uint8_t *buf, uint16_t len)
{
    uint32_t peek_len;

    if ((link == NULL) || (buf == NULL) || (len == 0U)) {
        return 0U;
    }

    __disable_irq();
    peek_len = ringbuf_peek(link->res->rx_fifo, buf, len);
    __enable_irq();

    return (uint16_t)peek_len;
}

uint16_t com_link_rx_drop(com_link_t *link, uint16_t len)
{
    uint32_t drop_len;

    if (link == NULL) {
        return 0U;
    }

    __disable_irq();
    drop_len = ringbuf_drop(link->res->rx_fifo, len);
    __enable_irq();

    return (uint16_t)drop_len;
}

void com_link_rx_clear(com_link_t *link)
{
    if (link == NULL) {
        return;
    }

    __disable_irq();
    ringbuf_reset(link->res->rx_fifo);
    __enable_irq();
}

static void com_link_rx_store(com_link_t *link)
{
    uint32_t remain;
    uint32_t rx_len;

    dma_channel_disable(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link));
    remain = dma_transfer_number_get(LINK_RX_DMA_CTL(link),
                                     LINK_RX_DMA_CH(link));
    if (remain <= link->res->rx_dma_size) {
        rx_len = (uint32_t)link->res->rx_dma_size - remain;
        if (rx_len > 0U) {
            (void)ringbuf_write(link->res->rx_fifo, link->res->rx_dma_buf, rx_len);
        }
    }

    com_link_restart_rx_dma(link);
}

void com_link_irq(com_link_t *link)
{
    if (link == NULL) {
        return;
    }

    if (usart_interrupt_flag_get(LINK_USART(link), USART_INT_FLAG_IDLE) == RESET) {
        return;
    }

    com_link_clear_idle_flag(link);
    if (link->initialized) {
        com_link_rx_store(link);
    }
}

void com_link_rx_dma_irq(com_link_t *link)
{
    if ((link == NULL) || !link->initialized ||
        (dma_flag_get(LINK_RX_DMA_CTL(link), LINK_RX_DMA_CH(link),
                      DMA_FLAG_FTF) == RESET)) {
        return;
    }

    com_link_rx_store(link);
}
