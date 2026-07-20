#ifndef IEC104_PORT_USART1_H
#define IEC104_PORT_USART1_H

#include "../core/iec104_apci.h"
#include "iec104_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GD32 USART1 + DMA implementation of the IEC104 transport port.
 * Implements the iec104_port_* interface declared in iec104_port.h.
 */
uint8_t iec104_port_usart1_init(void);

/* Interrupt vector entry points (must stay concretely named). */
void iec104_port_usart1_irq(void);
void iec104_port_usart1_rx_dma_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_PORT_USART1_H */
