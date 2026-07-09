#include "tf_485.h"

#include <string.h>

#include "gd32f10x_misc.h"
#include "tf_board_dma.h"
#include "tf_board_uart.h"
#include "tf_drv_io.h"
#include "tf_hal_dma.h"
#include "tf_hal_uart.h"
#include "../TFSDK/config/tf_board_mcu.h"

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#define TF_485_UART_ID      E_DRV_UART_2
#define TF_485_DMA_ID       E_DRV_DMA_1
#define TF_485_RX_DMA_CH    E_DRV_D0_CH2
#define TF_485_TX_DMA_CH    E_DRV_D0_CH4
#define TF_485_CTRL_RX      0U
#define TF_485_CTRL_TX      1U

static void *uart_base(void)
{
    return uart_configs[TF_485_UART_ID].base;
}

static void *dma_base(void)
{
    return dma_configs[TF_485_DMA_ID].base;
}

static uint8_t rx_dma_hw_ch(void)
{
    return (uint8_t)dma_configs[TF_485_DMA_ID].ch[TF_485_RX_DMA_CH].channel;
}

static uint8_t tx_dma_hw_ch(void)
{
    return (uint8_t)dma_configs[TF_485_DMA_ID].ch[TF_485_TX_DMA_CH].channel;
}

static void tf_485_ctrl_rx(tf_485_t *ctx)
{
    if ((ctx != NULL) && (ctx->hw.ctrl != NULL)) {
        tf_drv_io_set(ctx->hw.ctrl, TF_485_CTRL_RX);
    }
}

static void tf_485_ctrl_tx(tf_485_t *ctx)
{
    if ((ctx != NULL) && (ctx->hw.ctrl != NULL)) {
        tf_drv_io_set(ctx->hw.ctrl, TF_485_CTRL_TX);
    }
}

static void tf_485_stop_rx_dma(tf_485_t *ctx)
{
    tf_drv_dma_stop(ctx->hw.dma, TF_485_RX_DMA_CH);

    tf_hal_config_uart_dma_ch(uart_base(),
                              rx_dma_hw_ch(),
                              0U);
}

static void tf_485_restart_rx_dma(tf_485_t *ctx)
{
    tf_485_stop_rx_dma(ctx);

    tf_drv_dma_addr_config(ctx->hw.dma,
                           TF_485_RX_DMA_CH,
                           dma_configs[TF_485_DMA_ID].ch[TF_485_RX_DMA_CH].periph_addr,
                           (uint32_t)ctx->rx.dma_buf);
    tf_drv_dma_number_config(ctx->hw.dma, TF_485_RX_DMA_CH, MODULE_MODBUS_RX_BUF_SIZE);
    tf_hal_dma_flag_clear(dma_base(),
                          rx_dma_hw_ch(),
                          DMA_FLAG_G);
    tf_hal_uart_rx_flush(uart_base());
    tf_hal_config_uart_dma_ch(uart_base(),
                              rx_dma_hw_ch(),
                              1U);
    tf_drv_dma_start(ctx->hw.dma, TF_485_RX_DMA_CH);
}

bool tf_485_init(tf_485_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->hw.uart = tf_drv_uart_register(TF_485_UART_ID);
    ctx->hw.dma = tf_drv_dma_register(TF_485_DMA_ID);
    ctx->hw.ctrl = tf_drv_io_register(E_IO_485_CTRL);

    if ((ctx->hw.uart == NULL) || (ctx->hw.dma == NULL) ||
        (ctx->hw.ctrl == NULL)) {
        return false;
    }

    tf_485_ctrl_rx(ctx);
    ringbuf_init(&ctx->rx.fifo, ctx->rx.fifo_buf, sizeof(ctx->rx.fifo_buf));
    tf_485_restart_rx_dma(ctx);
    tf_hal_uart_interrupt_enable(uart_base(), USART_INT_IDLE);
    nvic_irq_enable(USART2_IRQn, 1U, 0U);

    return true;
}

void tf_485_reset(tf_485_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->tx.busy = false;
    tf_485_ctrl_rx(ctx);
    tf_485_rx_clear(ctx);
}

bool tf_485_tx_busy(const tf_485_t *ctx)
{
    return (ctx != NULL) ? ctx->tx.busy : false;
}

uint8_t *tf_485_tx_buffer(tf_485_t *ctx)
{
    return (ctx != NULL) ? ctx->tx.dma_buf : NULL;
}

void tf_485_tx_service(tf_485_t *ctx)
{
    if ((ctx == NULL) || !ctx->tx.busy) {
        return;
    }

    if ((tf_hal_dma_flag_get(dma_base(),
                             tx_dma_hw_ch(),
                             DMA_FLAG_FTF) == 0U) ||
        (tf_hal_uart_flag_get(uart_base(), USART_FLAG_TC) == 0U)) {
        return;
    }

    tf_hal_config_uart_dma_ch(uart_base(),
                              tx_dma_hw_ch(),
                              0U);
    tf_drv_dma_stop(ctx->hw.dma, TF_485_TX_DMA_CH);
    tf_hal_dma_flag_clear(dma_base(),
                          tx_dma_hw_ch(),
                          DMA_FLAG_G);
    tf_485_ctrl_rx(ctx);
    ctx->tx.busy = false;
}

bool tf_485_tx_start(tf_485_t *ctx, uint32_t len)
{
    if ((ctx == NULL) || (ctx->hw.uart == NULL) || (ctx->hw.dma == NULL) ||
        (ctx->hw.ctrl == NULL) || (len == 0U) ||
        (len > MODULE_MODBUS_TX_BUF_SIZE) || ctx->tx.busy) {
        return false;
    }

    tf_hal_config_uart_dma_ch(uart_base(),
                              tx_dma_hw_ch(),
                              0U);
    tf_drv_dma_stop(ctx->hw.dma, TF_485_TX_DMA_CH);
    tf_drv_dma_addr_config(ctx->hw.dma,
                           TF_485_TX_DMA_CH,
                           dma_configs[TF_485_DMA_ID].ch[TF_485_TX_DMA_CH].periph_addr,
                           (uint32_t)ctx->tx.dma_buf);
    tf_drv_dma_number_config(ctx->hw.dma, TF_485_TX_DMA_CH, len);
    tf_hal_dma_flag_clear(dma_base(),
                          tx_dma_hw_ch(),
                          DMA_FLAG_G);
    tf_hal_uart_flag_clear(uart_base(), USART_FLAG_TC);
    tf_485_ctrl_tx(ctx);
    tf_hal_config_uart_dma_ch(uart_base(),
                              tx_dma_hw_ch(),
                              1U);
    tf_drv_dma_start(ctx->hw.dma, TF_485_TX_DMA_CH);
    ctx->tx.busy = true;

    return true;
}

bool tf_485_send(tf_485_t *ctx, const uint8_t *data, uint32_t len)
{
    if ((ctx == NULL) || (data == NULL) || (len == 0U) ||
        (len > MODULE_MODBUS_TX_BUF_SIZE) || ctx->tx.busy) {
        return false;
    }

    memcpy(ctx->tx.dma_buf, data, len);
    return tf_485_tx_start(ctx, len);
}

void tf_485_rx_clear(tf_485_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    __disable_irq();
    ringbuf_reset(&ctx->rx.fifo);
    __enable_irq();
}

uint32_t tf_485_rx_available(tf_485_t *ctx)
{
    uint32_t available;

    if (ctx == NULL) {
        return 0U;
    }

    __disable_irq();
    available = ringbuf_available(&ctx->rx.fifo);
    __enable_irq();

    return available;
}

uint32_t tf_485_rx_read(tf_485_t *ctx,
                        uint8_t *buf,
                        uint32_t buf_size)
{
    uint32_t len;

    if ((ctx == NULL) || (buf == NULL) || (buf_size == 0U)) {
        return 0U;
    }

    __disable_irq();
    len = ringbuf_read(&ctx->rx.fifo, buf, buf_size);
    __enable_irq();

    return len;
}

uint32_t tf_485_rx_peek(tf_485_t *ctx,
                        uint8_t *buf,
                        uint32_t len)
{
    uint32_t peek_len;

    if ((ctx == NULL) || (buf == NULL) || (len == 0U)) {
        return 0U;
    }

    __disable_irq();
    peek_len = ringbuf_peek(&ctx->rx.fifo, buf, len);
    __enable_irq();

    return peek_len;
}

uint32_t tf_485_rx_drop(tf_485_t *ctx, uint32_t len)
{
    uint32_t drop_len;

    if (ctx == NULL) {
        return 0U;
    }

    __disable_irq();
    drop_len = ringbuf_drop(&ctx->rx.fifo, len);
    __enable_irq();

    return drop_len;
}

uint32_t tf_485_rx_overflow_count(tf_485_t *ctx)
{
    uint32_t overflow;

    if (ctx == NULL) {
        return 0U;
    }

    __disable_irq();
    overflow = ringbuf_overflow_count(&ctx->rx.fifo);
    __enable_irq();

    return overflow;
}

void tf_485_irq(tf_485_t *ctx)
{
    uint32_t remain;
    uint32_t rx_len;

    if ((ctx == NULL) || (ctx->hw.uart == NULL) || (ctx->hw.dma == NULL)) {
        tf_hal_uart_idle_clear(uart_base());
        return;
    }

    if (tf_hal_uart_interrupt_flag_get(uart_base(),
                                       USART_INT_FLAG_IDLE) != 0U) {
        tf_hal_uart_idle_clear(uart_base());
        tf_drv_dma_stop(ctx->hw.dma, TF_485_RX_DMA_CH);

        remain = tf_drv_dma_remaining_get(ctx->hw.dma, TF_485_RX_DMA_CH);
        rx_len = MODULE_MODBUS_RX_BUF_SIZE - remain;

        if ((rx_len > 0U) && (rx_len <= MODULE_MODBUS_RX_BUF_SIZE)) {
            (void)ringbuf_write(&ctx->rx.fifo, ctx->rx.dma_buf, rx_len);
        }

        tf_485_restart_rx_dma(ctx);
    }
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
