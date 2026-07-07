#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "uart_and_dma.h"
#include "../TFSDK/config/tf_board_dma.h"
#include "../TFSDK/config/tf_board_uart.h"
#include "../TFSDK/framework/tf_debug.h"
#include "../TFSDK/hal/tf_hal_dma.h"
#include "../TFSDK/hal/tf_hal_uart.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_usart.h"


static tf_drv_uart_handle_t h_uart0 = NULL;
static tf_drv_uart_handle_t h_uart1 = NULL;
static tf_drv_uart_handle_t h_uart2 = NULL;
static tf_drv_dma_handle_t h_dma0 = NULL;
static tf_drv_dma_handle_t h_dma1 = NULL;

/* ── UART0 ping-pong 缓冲区 ── */
#define PP_IDLE  (0xFFU)

static uint8_t  pp_using_idx = PP_IDLE;  /* 0=bufa活动中, 1=bufb活动中, PP_IDLE=空闲 */
static uint8_t  pp_buf[2][512];   /* [0]=bufa, [1]=bufb */
static uint16_t pp_using_buf_data_len[2];        /* 各缓冲区待发数据长度 */

static void tf_debug_console_write(const char *buf, uint32_t len)
{
    uint32_t tx_len = 0U;
    uint8_t  dma_ch;
    void    *base;
    int      idle;

    if (h_dma0 == NULL) return;

    tf_drv_uart_id_e using_uart_id = E_DRV_UART_0;
    tf_drv_dma_id_e using_dma_id = E_DRV_DMA_0;
    tf_drv_dma_ch_id_e using_dma_ch_id = E_DRV_D0_CH3;

    dma_ch = (uint8_t)dma_configs[using_dma_id].ch[using_dma_ch_id].channel;
    base   = dma_configs[using_dma_id].base;

    /* ① FTF → 释放刚传完的缓冲区 */
    if ((pp_using_idx != PP_IDLE) &&
        (tf_hal_dma_flag_get(base, dma_ch, DMA_FLAG_FTF) != 0U)) {
        pp_using_buf_data_len[pp_using_idx] = 0U;
        pp_using_idx = PP_IDLE;
    }

    /* ② DMA 空闲且对面有积压 → 切换发出 */
    if (pp_using_idx == PP_IDLE) {
        int pend = (pp_using_buf_data_len[0] > 0U) ? 0 : (pp_using_buf_data_len[1] > 0U) ? 1 : -1;
        if (pend >= 0) {
            pp_using_idx = pend;
            goto send;
        }
    }

    /* ③ 写新数据到空闲缓冲区末尾 */
    idle = (pp_using_idx == PP_IDLE) ? 0 : (1 - (int)pp_using_idx);
    if (pp_using_buf_data_len[idle] >= (uint16_t)sizeof(pp_buf[idle])) {
        /* 缓冲区满 → DMA 阻塞 → 强制切换，发送积压数据 */
        if (pp_using_idx != PP_IDLE) {
            pp_using_buf_data_len[pp_using_idx] = 0U;           /* 丢弃旧传输 */
        }
        pp_using_idx = (uint8_t)idle;
        goto send;
    }

    {
        uint16_t off   = pp_using_buf_data_len[idle];
        uint16_t space = (uint16_t)sizeof(pp_buf[idle]) - off;

        for (uint32_t i = 0; i < len; ++i) {
            if (tx_len >= space) break;

            if ((buf[i] == '\n') && ((i == 0U) || (buf[i - 1U] != '\r'))) {
                if (tx_len + 1U >= space) break;
                pp_buf[idle][off + tx_len++] = (uint8_t)'\r';
            }

            pp_buf[idle][off + tx_len++] = (uint8_t)buf[i];
        }
        if (tx_len == 0U) return;
        pp_using_buf_data_len[idle] = off + (uint16_t)tx_len;
    }

    /* ④ DMA 空闲 → 发出 */
    if (pp_using_idx == PP_IDLE) {
        pp_using_idx = (uint8_t)idle;
        goto send;
    }
    return;

send:
    tf_hal_config_uart_dma_ch(uart_configs[using_uart_id].base,
                            (uint8_t)dma_configs[using_dma_id].ch[using_dma_ch_id].channel, 0);
    tf_drv_dma_stop(h_dma0, using_dma_ch_id);
    tf_drv_dma_addr_config(h_dma0, using_dma_ch_id,
        dma_configs[using_dma_id].ch[using_dma_ch_id].periph_addr,
        (uint32_t)pp_buf[pp_using_idx]);
    tf_drv_dma_number_config(h_dma0, using_dma_ch_id, pp_using_buf_data_len[pp_using_idx]);
    tf_hal_dma_flag_clear(dma_configs[using_dma_id].base, dma_configs[using_dma_id].ch[using_dma_ch_id].channel,DMA_FLAG_G);
    tf_hal_config_uart_dma_ch(uart_configs[using_uart_id].base,
                            (uint8_t)dma_configs[using_dma_id].ch[using_dma_ch_id].channel, 1);
    tf_drv_dma_start(h_dma0, using_dma_ch_id);
}

int tf_debug_printf(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    int len;

    va_start(ap, fmt);
#if MCU_CONTAIN_PRE_USART
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len <= 0) {
        return len;
    }

    tf_debug_console_write(buf, (len < (int)sizeof(buf)) ? (uint32_t)len : (uint32_t)(sizeof(buf) - 1U));
    return len;
#else
    len = vprintf(fmt, ap);
    va_end(ap);
    return len;
#endif
}


void use_uart_and_dma(void)
{
    h_uart0 = tf_drv_uart_register(E_DRV_UART_0);
    h_uart1 = tf_drv_uart_register(E_DRV_UART_1);
    h_uart2 = tf_drv_uart_register(E_DRV_UART_2);
    h_dma0 = tf_drv_dma_register(E_DRV_DMA_0);
    h_dma1 = tf_drv_dma_register(E_DRV_DMA_1);

    if ((h_uart0 != NULL) && (h_dma0 != NULL)) {
        tf_hal_config_uart_dma_ch(uart_configs[E_DRV_UART_0].base,
                            (uint8_t)dma_configs[E_DRV_DMA_0].ch[E_DRV_D0_CH3].channel, 1);
    }

    

    TF_PRINTF("[APP   ] USART0 console ready\n");
    TF_DEBUG("APP", "TF_PRINTF/TF_DEBUG use USART0 DMA0_CH3 polling TX\n");

    

    if ((h_uart1 != NULL) && (h_dma0 != NULL)) {
        tf_hal_config_uart_dma_ch(uart_configs[E_DRV_UART_1].base,
                                (uint8_t)dma_configs[E_DRV_DMA_0].ch[E_DRV_D0_CH5].channel, 1);
        tf_hal_config_uart_dma_ch(uart_configs[E_DRV_UART_1].base,
                                (uint8_t)dma_configs[E_DRV_DMA_0].ch[E_DRV_D0_CH6].channel, 1);
    }

    if ((h_uart2 != NULL) && (h_dma1 != NULL)) {
        tf_hal_config_uart_dma_ch(uart_configs[E_DRV_UART_2].base,
                                (uint8_t)dma_configs[E_DRV_DMA_1].ch[E_DRV_D0_CH4].channel, 1);
        tf_hal_config_uart_dma_ch(uart_configs[E_DRV_UART_2].base,
                                (uint8_t)dma_configs[E_DRV_DMA_1].ch[E_DRV_D0_CH2].channel, 1);
    }
}
