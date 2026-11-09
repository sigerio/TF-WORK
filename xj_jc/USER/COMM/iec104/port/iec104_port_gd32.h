#ifndef IEC104_PORT_GD32_H
#define IEC104_PORT_GD32_H

#include "COMM/iec104/core/iec104_apci.h"
#include "COMM/iec104/port/iec104_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IEC104 传输端口的 GD32 USART1 + DMA 实现。
 * USART1 直接承载连续 APDU 字节流，接收 DMA 分段写入软件 FIFO。
 */
uint8_t iec104_port_usart1_init(void);

/* 由芯片中断向量直接调用的 USART1 空闲中断和 RX DMA 满传中断入口。 */
void iec104_port_usart1_irq(void);
void iec104_port_usart1_rx_dma_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_PORT_GD32_H */
