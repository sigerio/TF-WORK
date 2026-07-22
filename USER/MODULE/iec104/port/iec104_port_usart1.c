#include "iec104_port_usart1.h"

#include <string.h>

#include "gd32f10x_dma.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_usart.h"
#include "ringbuf.h"
#include "tf_board_dma.h"
#include "tf_board_uart.h"
#include "tf_drv_dma.h"
#include "tf_drv_uart.h"
#include "tf_hal_dma.h"
#include "tf_hal_uart.h"
#include "../../../../TFSDK/config/tf_board_mcu.h"

#if (MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA)

#define IEC104_RX_DMA_SIZE   256U
#define IEC104_RX_FIFO_SIZE  1024U

typedef struct {
    tf_drv_uart_id_e uart;
    tf_drv_dma_id_e dma;
    tf_drv_dma_ch_id_e rx_dma;
    tf_drv_dma_ch_id_e tx_dma;
} iec104_port_usart1_hw_t;

static const iec104_port_usart1_hw_t hw = {
    .uart = E_DRV_UART_1,
    .dma = E_DRV_DMA_0,
    .rx_dma = E_DRV_DX_CH5,
    .tx_dma = E_DRV_DX_CH6,
};

typedef struct {
    uint8_t dma_buf[IEC104_RX_DMA_SIZE];
    uint8_t fifo_buf[IEC104_RX_FIFO_SIZE];
    ringbuf_t fifo;
} iec104_rx_t;

typedef struct {
    uint8_t dma_buf[IEC104_MAX_FRAME_LENGTH];
    uint8_t tx_busy;
} iec104_tx_t;

static tf_drv_uart_handle_t uart;
static tf_drv_dma_handle_t dma;
static iec104_rx_t rx;
static iec104_tx_t tx;

static void iec104_rx_dma_restart(void)
{
    /* 每次提交本批数据后，从 DMA 缓冲区起点重新接收下一批字节。 */
    tf_drv_dma_stop(dma, hw.rx_dma);
    tf_hal_config_uart_dma_ch(uart_configs[hw.uart].base, (uint8_t)dma_configs[hw.dma].ch[hw.rx_dma].channel, 0U);
    tf_drv_dma_addr_config(dma, hw.rx_dma, dma_configs[hw.dma].ch[hw.rx_dma].periph_addr, (uint32_t)rx.dma_buf);
    tf_drv_dma_number_config(dma, hw.rx_dma, IEC104_RX_DMA_SIZE);
    tf_hal_dma_flag_clear(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.rx_dma].channel, DMA_FLAG_G);
    tf_hal_config_uart_dma_ch(uart_configs[hw.uart].base, (uint8_t)dma_configs[hw.dma].ch[hw.rx_dma].channel, 1U);
    tf_drv_dma_start(dma, hw.rx_dma);
}

static void iec104_rx_dma_store(void)
{
    uint32_t remain;
    uint32_t rx_length;

    /*
     * USART 空闲中断提交不足 256 字节的短批次，DMA 满中断提交完整批次；
     * 两条中断路径共用本函数，提交后立即重启 DMA。
     */
    tf_drv_dma_stop(dma, hw.rx_dma);
    remain = tf_drv_dma_remaining_get(dma, hw.rx_dma);
    if (remain <= IEC104_RX_DMA_SIZE) {
        rx_length = IEC104_RX_DMA_SIZE - remain;
        if (rx_length > 0U) {
            (void)ringbuf_write(&rx.fifo, rx.dma_buf, rx_length);
        }
    }
    iec104_rx_dma_restart();
}

uint8_t *iec104_port_tx_buffer(void)
{
    return ((uart != NULL) && (dma != NULL)) ? tx.dma_buf : NULL;
}

uint8_t iec104_port_tx_busy(void)
{
    return tx.tx_busy;
}

uint8_t iec104_port_tx_start(uint16_t apdu_length)
{
    if ((uart == NULL) || (dma == NULL) ||
        tx.tx_busy || (apdu_length == 0U) ||
        (apdu_length > IEC104_MAX_FRAME_LENGTH)) {
        return 0U;
    }

    tf_hal_config_uart_dma_ch(uart_configs[hw.uart].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, 0U);
    tf_drv_dma_stop(dma, hw.tx_dma);
    tf_drv_dma_addr_config(dma, hw.tx_dma, dma_configs[hw.dma].ch[hw.tx_dma].periph_addr, (uint32_t)tx.dma_buf);
    tf_drv_dma_number_config(dma, hw.tx_dma, apdu_length);
    tf_hal_dma_flag_clear(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, DMA_FLAG_G);
    tf_hal_uart_flag_clear(uart_configs[hw.uart].base, USART_FLAG_TC);
    tf_hal_config_uart_dma_ch(uart_configs[hw.uart].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, 1U);
    tf_drv_dma_start(dma, hw.tx_dma);
    tx.tx_busy = 1U;

    return 1U;
}

void iec104_port_tx_service(void)
{
    if ((uart == NULL) || (dma == NULL) || (tx.tx_busy == 0U)) {
        return;
    }

    if ((tf_hal_dma_flag_get(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, DMA_FLAG_FTF) == 0U) ||
        (tf_hal_uart_flag_get(uart_configs[hw.uart].base, USART_FLAG_TC) == 0U)) {
        return;
    }

    /* DMA 搬运完成且 USART 移位寄存器发送完成后，缓冲区才可复用。 */
    tf_hal_config_uart_dma_ch(uart_configs[hw.uart].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, 0U);
    tf_drv_dma_stop(dma, hw.tx_dma);
    tf_hal_dma_flag_clear(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.tx_dma].channel, DMA_FLAG_G);
    tx.tx_busy = 0U;
}

uint8_t iec104_port_usart1_init(void)
{
    uart = NULL;
    dma = NULL;
    memset(&rx, 0, sizeof(rx));
    memset(&tx, 0, sizeof(tx));
    uart = tf_drv_uart_register(hw.uart);
    dma = tf_drv_dma_register(hw.dma);

    if ((uart == NULL) || (dma == NULL)) {
        return 0U;
    }

    ringbuf_init(&rx.fifo, rx.fifo_buf, sizeof(rx.fifo_buf));
    tf_hal_uart_rx_flush(uart_configs[hw.uart].base);
    iec104_rx_dma_restart();
    /* IDLE 负责短帧/停顿分段，DMA FTF 保证连续满缓冲数据也能及时提交。 */
    tf_hal_dma_interrupt_config(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.rx_dma].channel, DMA_INT_FTF);
    tf_hal_uart_interrupt_enable(uart_configs[hw.uart].base, USART_INT_IDLE);
    nvic_irq_enable(DMA0_Channel5_IRQn, 1U, 0U);
    nvic_irq_enable(USART1_IRQn, 1U, 0U);

    return 1U;
}

uint16_t iec104_port_rx_available(void)
{
    uint32_t available;

    if ((uart == NULL) || (dma == NULL)) {
        return 0U;
    }

    __disable_irq();
    available = ringbuf_available(&rx.fifo);
    __enable_irq();

    return (uint16_t)available;
}

uint8_t iec104_port_rx_peek(uint8_t *data, uint16_t length)
{
    uint32_t peek_length = 0U;

    if ((uart == NULL) || (dma == NULL) || (data == NULL) || (length == 0U)) {
        return 0U;
    }

    __disable_irq();
    if (ringbuf_available(&rx.fifo) >= length) {
        peek_length = ringbuf_peek(&rx.fifo, data, length);
    }
    __enable_irq();

    return peek_length == length;
}

uint8_t iec104_port_rx_read(uint8_t *data, uint16_t length)
{
    uint32_t read_length = 0U;

    if ((uart == NULL) || (dma == NULL) || (data == NULL) || (length == 0U)) {
        return 0U;
    }

    __disable_irq();
    if (ringbuf_available(&rx.fifo) >= length) {
        read_length = ringbuf_read(&rx.fifo, data, length);
    }
    __enable_irq();

    return read_length == length;
}

void iec104_port_usart1_irq(void)
{
    if (tf_hal_uart_interrupt_flag_get(uart_configs[hw.uart].base, USART_INT_FLAG_IDLE) == 0U) {
        return;
    }

    tf_hal_uart_idle_clear(uart_configs[hw.uart].base);
    if ((uart == NULL) || (dma == NULL)) {
        return;
    }

    iec104_rx_dma_store();
}

void iec104_port_usart1_rx_dma_irq(void)
{
    if ((uart == NULL) || (dma == NULL) ||
        (tf_hal_dma_flag_get(dma_configs[hw.dma].base, (uint8_t)dma_configs[hw.dma].ch[hw.rx_dma].channel, DMA_FLAG_FTF) == 0U)) {
        return;
    }

    iec104_rx_dma_store();
}

#endif /* MCU_CONTAIN_PRE_USART && MCU_CONTAIN_PRE_DMA */
