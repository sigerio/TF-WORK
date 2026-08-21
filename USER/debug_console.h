#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include <stdint.h>

void debug_console_init(void);
void debug_console_irq(void);
void debug_console_tx_dma_irq(void);
int tf_debug_printf(const char *format, ...);

/* 上位机命令接收（RXNE 中断推入 FIFO，供调试命令解析读取） */
uint16_t debug_console_rx_available(void);
uint16_t debug_console_rx_read(uint8_t *buf, uint16_t len);

#endif /* DEBUG_CONSOLE_H */
