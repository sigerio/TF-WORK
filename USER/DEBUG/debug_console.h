#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdint.h>

/* 初始化调试串口及其收发 DMA；仅在 USART2 处于 DEBUG 角色时使用。 */
void debug_console_init(void);
/* USART2 和 TX DMA 的中断转发入口。 */
void debug_console_irq(void);
void debug_console_tx_dma_irq(void);
/* 格式化并提交调试文本，返回 vsnprintf 结果；调试功能关闭时返回 0。 */
int debug_printf(const char *format, ...);

void debug_console_task(void);

#endif /* DEBUG_CONSOLE_H */
