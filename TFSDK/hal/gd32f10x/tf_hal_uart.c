/**
 * @file    tf_hal_uart.c
 * @brief   GD32F10x UART HAL 薄封装
 *
 * 基于 GD32F10x Firmware Library 的 USART 驱动实现。
 * GPIO 初始化由 IO 子系统完成，此处只负责 USART 时钟、外设本体配置
 * 以及 UART 与 DMA 通道请求链路建立。
 */

#include "gd32f10x.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"
#include "../tf_hal_uart.h"
#include "../../drv/tf_drv_uart.h"
#include "../../config/tf_board_config.h"

#if MCU_CONTAIN_PRE_USART

static void gd32_uart_clock_enable(uint32_t periph)
{
    switch (periph) {
    case USART0:
        rcu_periph_clock_enable(RCU_USART0);
        break;
    case USART1:
        rcu_periph_clock_enable(RCU_USART1);
        break;
    case USART2:
        rcu_periph_clock_enable(RCU_USART2);
        break;
    
    default:
        break;
    }
}



static void set_uart_baudrate(uint32_t periph, uint32_t baudrate)
{
    usart_baudrate_set(periph, baudrate);
}


void tf_hal_uart_init(const tf_drv_uart_config_t *cfg)
{
    uint32_t periph = (uint32_t)cfg->base;

    gd32_uart_clock_enable(periph);

    usart_deinit(periph);
    set_uart_baudrate(periph, cfg->baudrate);
    usart_word_length_set(periph, USART_WL_8BIT);
    usart_stop_bit_set(periph, USART_STB_1BIT);
    usart_parity_config(periph, USART_PM_NONE);
    usart_hardware_flow_rts_config(periph, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(periph, USART_CTS_DISABLE);
    usart_receive_config(periph, USART_RECEIVE_ENABLE);
    usart_transmit_config(periph, USART_TRANSMIT_ENABLE);
    usart_enable(periph);
}

void tf_hal_uart_enable(void *base)
{
    usart_enable((uint32_t)base);
}

void tf_hal_uart_disable(void *base)
{
    usart_disable((uint32_t)base);
}

void tf_hal_uart_send_byte(void *base, uint8_t data)
{
    uint32_t periph = (uint32_t)base;
    usart_data_transmit(periph, data);
    while (RESET == usart_flag_get(periph, USART_FLAG_TBE)) {
    }
}

uint8_t tf_hal_uart_receive_byte(void *base)
{
    uint32_t periph = (uint32_t)base;
    while (RESET == usart_flag_get(periph, USART_FLAG_RBNE)) {
    }

    return (uint8_t)usart_data_receive(periph);
}

void tf_hal_uart_tx(void *base, const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }

    uint32_t periph = (uint32_t)base;

    for (uint32_t i = 0; i < len; ++i) {
        usart_data_transmit(periph, data[i]);
        while (RESET == usart_flag_get(periph, USART_FLAG_TBE)) {
        }
    }

    while (RESET == usart_flag_get(periph, USART_FLAG_TC)) {
    }
}

void tf_hal_uart_rx(void *base, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0U) {
        return;
    }

    uint32_t periph = (uint32_t)base;

    for (uint32_t i = 0; i < len; ++i) {
        while (RESET == usart_flag_get(periph, USART_FLAG_RBNE)) {
        }
        buf[i] = (uint8_t)usart_data_receive(periph);
    }
}

uint32_t tf_hal_uart_flag_get(void *base, uint32_t flag)
{
    uint32_t periph = (uint32_t)base;
    return (usart_flag_get(periph, (usart_flag_enum)flag) != RESET) ? flag : 0U;
}

void tf_hal_uart_flag_clear(void *base, uint32_t flag)
{
    usart_flag_clear((uint32_t)base, (usart_flag_enum)flag);
}

void tf_hal_uart_interrupt_enable(void *base, uint32_t interrupt)
{
    usart_interrupt_enable((uint32_t)base, interrupt);
}

void tf_hal_uart_interrupt_disable(void *base, uint32_t interrupt)
{
    usart_interrupt_disable((uint32_t)base, interrupt);
}

uint32_t tf_hal_uart_interrupt_flag_get(void *base, uint32_t int_flag)
{
    uint32_t periph = (uint32_t)base;
    return (usart_interrupt_flag_get(periph, int_flag) != RESET) ? int_flag : 0U;
}

uint32_t tf_hal_uart_stat_get(void *base)
{
    return USART_STAT((uint32_t)base);
}

uint8_t tf_hal_uart_data_read(void *base)
{
    return (uint8_t)(USART_DATA((uint32_t)base) & USART_DATA_DATA);
}

void tf_hal_uart_idle_clear(void *base)
{
    volatile uint32_t stat;
    volatile uint32_t data;

    stat = USART_STAT((uint32_t)base);
    data = USART_DATA((uint32_t)base);
    (void)stat;
    (void)data;
}

void tf_hal_uart_rx_flush(void *base)
{
    uint32_t periph = (uint32_t)base;
    volatile uint32_t data;

    tf_hal_uart_idle_clear(base);

    while (usart_flag_get(periph, USART_FLAG_RBNE) != RESET) {
        data = USART_DATA(periph);
        (void)data;
    }
}

void tf_hal_config_uart_dma_ch(void *base, uint8_t dma_channel, uint8_t en)
{
    uint32_t periph = (uint32_t)base;

    if (((periph == USART0) && (dma_channel == DMA_CH3)) ||
        ((periph == USART1) && (dma_channel == DMA_CH6)) ||
        ((periph == USART2) && (dma_channel == DMA_CH4))) {
        if(en)
            usart_dma_transmit_config(periph, USART_TRANSMIT_DMA_ENABLE);
        else
            usart_dma_transmit_config(periph, USART_TRANSMIT_DMA_DISABLE);
        return;
    }

    if (((periph == USART0) && (dma_channel == DMA_CH4)) ||
        ((periph == USART1) && (dma_channel == DMA_CH5)) ||
        ((periph == USART2) && (dma_channel == DMA_CH2))) {
        if(en)
            usart_dma_receive_config(periph, USART_RECEIVE_DMA_ENABLE);
        else
            usart_dma_receive_config(periph, USART_RECEIVE_DMA_DISABLE);
        return;
    }
}


#endif /* MCU_CONTAIN_PRE_USART */
