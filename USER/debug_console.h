#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdint.h>

/* 初始化调试串口及其收发 DMA；仅在 USART2 处于 DEBUG 角色时使用。 */
void debug_console_init(void);
/* USART2 和 TX DMA 的中断转发入口。 */
void debug_console_irq(void);
void debug_console_tx_dma_irq(void);
/* 格式化并提交调试文本，返回 vsnprintf 结果；调试功能关闭时返回 0。 */
int tf_debug_printf(const char *format, ...);

/* 上位机命令接收（RXNE 中断推入 FIFO，供调试命令解析读取） */
uint16_t debug_console_rx_available(void);
uint16_t debug_console_rx_read(uint8_t *buf, uint16_t len);

#endif /* DEBUG_CONSOLE_H */
