#ifndef TF_HAL_UART_H
#define TF_HAL_UART_H

#include <stdint.h>

typedef struct tf_drv_uart_config tf_drv_uart_config_t;

void tf_hal_uart_init(const tf_drv_uart_config_t *cfg);
void tf_hal_uart_tx(void *base, const uint8_t *data, uint32_t len);
void tf_hal_uart_rx(void *base, uint8_t *buf, uint32_t len);
uint32_t tf_hal_uart_flag_get(void *base, uint32_t flag);
uint32_t tf_hal_uart_stat_get(void *base);
uint8_t tf_hal_uart_data_read(void *base);
void tf_hal_uart_flag_clear(void *base, uint32_t flag);
void tf_hal_uart_interrupt_enable(void *base, uint32_t interrupt);
void tf_hal_uart_interrupt_disable(void *base, uint32_t interrupt);
uint32_t tf_hal_uart_interrupt_flag_get(void *base, uint32_t int_flag);
void tf_hal_uart_idle_clear(void *base);
void tf_hal_uart_rx_flush(void *base);
/**
 * @brief 建立 UART 与 DMA 通道的请求链路
 *
 * 该接口由用户按平台资源映射自行维护调用时机。
 * 若某个 UART 需要启用 DMA，务必在完成 UART/DMA 初始化后调用本接口，
 * 以使能对应 UART 的 DMA 请求位。
 * 当前 GD32F103 工程已约定:
 *   USART0_TX -> DMA0_CH3
 *   USART1_TX -> DMA0_CH6, USART1_RX -> DMA0_CH5
 *   USART2_TX -> DMA1_CH4, USART2_RX -> DMA1_CH2
 * 其中 USART2 对应常见语义 USART3。
 *
 * @param base         UART 外设基地址
 * @param dma_channel  DMA 通道号（平台原生通道号）
 * @param en           DMA 通道号使能状态
 */
void tf_hal_config_uart_dma_ch(void *base, uint8_t dma_channel, uint8_t en);

#endif /* TF_HAL_UART_H */
