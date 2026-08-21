#include "iec104_port_usart1.h"

#include <string.h>

#include "board.h"
#include "com_link.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"
#include "ringbuf.h"

/*
 * IEC104 传输端口 — 基于 com_link 公共收发层的薄封装。
 *
 * 本链路（USART1，主板通信）无 485 方向脚，com_link 自动跳过方向控制。
 * 缓冲区与 FIFO 由本模块自行 static 分配并注册进 com_link，
 * 收发控制逻辑全部复用 com_link，本文件只做 iec104_port_* 接口转发。
 */

#define IEC104_RX_DMA_SIZE 256U
#define IEC104_RX_FIFO_SIZE 1024U

/* ── 模块自有缓冲与 FIFO（com_link 不分配内存） ── */
static uint8_t   s_rx_dma_buf[IEC104_RX_DMA_SIZE];
static uint8_t   s_rx_fifo_buf[IEC104_RX_FIFO_SIZE];
static ringbuf_t s_rx_fifo;
static uint8_t   s_tx_dma_buf[IEC104_MAX_FRAME_LENGTH];

static com_link_t s_link;

static const com_link_resource_t s_resource = {
    .usart       = &board_usart_configs[BOARD_USART_MAINBOARD],
    .rx_dma_buf  = s_rx_dma_buf,
    .rx_dma_size = (uint16_t)sizeof(s_rx_dma_buf),
    .rx_fifo     = &s_rx_fifo,
    .tx_dma_buf  = s_tx_dma_buf,
    .tx_dma_size = (uint16_t)sizeof(s_tx_dma_buf),
};

/* 谁用谁初始化：本口（USART1）硬件初始化，全部使用官方库函数 */
static void iec104_port_usart1_hardware_init(const board_usart_config_t *usart)
{
    const board_gpio_config_t *tx_pin = &board_gpio_configs[usart->tx_pin];
    const board_gpio_config_t *rx_pin = &board_gpio_configs[usart->rx_pin];
    const board_dma_config_t *dma = &board_dma_configs[usart->rx_dma.dma];
    const board_dma_channel_config_t *rx_dma = &dma->ch[usart->rx_dma.ch];
    const board_dma_channel_config_t *tx_dma = &dma->ch[usart->tx_dma.ch];
    dma_parameter_struct dma_parameters;

    /* GPIO */
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(tx_pin->port, tx_pin->mode, tx_pin->speed, tx_pin->pin);
    gpio_bit_write(tx_pin->port, tx_pin->pin, tx_pin->initial_level ? SET : RESET);
    gpio_init(rx_pin->port, rx_pin->mode, rx_pin->speed, rx_pin->pin);

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

    /* RX/TX DMA */
    rcu_periph_clock_enable(RCU_DMA0);
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
    dma_flag_clear(dma->controller, rx_dma->channel,
                   DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);

    dma_parameters.periph_addr = tx_dma->peripheral_address;
    dma_parameters.direction = tx_dma->direction;
    dma_deinit(dma->controller, tx_dma->channel);
    dma_init(dma->controller, tx_dma->channel, &dma_parameters);
    dma_circulation_disable(dma->controller, tx_dma->channel);
    dma_memory_to_memory_disable(dma->controller, tx_dma->channel);
    dma_flag_clear(dma->controller, tx_dma->channel,
                   DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);

    usart_dma_receive_config(usart->peripheral, USART_RECEIVE_DMA_ENABLE);
    usart_dma_transmit_config(usart->peripheral, USART_TRANSMIT_DMA_ENABLE);
}

uint8_t iec104_port_usart1_init(void)
{
    const board_usart_config_t *usart = &board_usart_configs[BOARD_USART_MAINBOARD];
    const board_dma_config_t *dma = &board_dma_configs[usart->rx_dma.dma];
    const board_dma_channel_config_t *rx_dma = &dma->ch[usart->rx_dma.ch];

    /* 谁用谁初始化：本口硬件 + NVIC 由本模块负责（官方库函数） */
    iec104_port_usart1_hardware_init(usart);
    /* RX DMA 满传中断：长帧/连续流撑满 DMA buf 时兜底推入 FIFO（IDLE 之外的第二保险） */
    dma_interrupt_enable(dma->controller, rx_dma->channel, DMA_INT_FTF);
    nvic_irq_enable(DMA0_Channel5_IRQn, 1U, 1U);
    nvic_irq_enable(com_link_usart_irqn(usart->peripheral), 1U, 0U);

    ringbuf_init(&s_rx_fifo, s_rx_fifo_buf, sizeof(s_rx_fifo_buf));
    return com_link_open(&s_link, &s_resource) ? 1U : 0U;
}

uint8_t *iec104_port_tx_buffer(void)
{
    return com_link_tx_buffer(&s_link);
}

uint8_t iec104_port_tx_busy(void)
{
    return com_link_tx_busy(&s_link) ? 1U : 0U;
}

uint8_t iec104_port_tx_start(uint16_t apdu_length)
{
    return com_link_tx_start(&s_link, apdu_length) ? 1U : 0U;
}

void iec104_port_tx_service(void)
{
    com_link_tx_service(&s_link);
}

uint16_t iec104_port_rx_available(void)
{
    return com_link_rx_available(&s_link);
}

uint8_t iec104_port_rx_peek(uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U)) {
        return 0U;
    }

    return (com_link_rx_peek(&s_link, data, length) == length) ? 1U : 0U;
}

uint8_t iec104_port_rx_read(uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U)) {
        return 0U;
    }

    return (com_link_rx_read(&s_link, data, length) == length) ? 1U : 0U;
}

void iec104_port_usart1_irq(void)
{
    com_link_irq(&s_link);
}

void iec104_port_usart1_rx_dma_irq(void)
{
    com_link_rx_dma_irq(&s_link);
}
