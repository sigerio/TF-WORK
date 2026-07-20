#ifndef IEC104_PORT_H
#define IEC104_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IEC104 transport port interface.
 *
 * These functions decouple the protocol session layer (iec104_server) from the
 * physical link. A concrete implementation (e.g. iec104_port_usart1) provides
 * these symbols; porting to another peripheral or platform only requires a new
 * iec104_port_xxx translation unit that implements this interface.
 */

/* Transmit side */
uint8_t *iec104_port_tx_buffer(void);
uint8_t iec104_port_tx_busy(void);
uint8_t iec104_port_tx_start(uint16_t apdu_length);
void iec104_port_tx_service(void);

/* Receive side */
uint16_t iec104_port_rx_available(void);
uint8_t iec104_port_rx_peek(uint8_t *data, uint16_t length);
uint8_t iec104_port_rx_read(uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_PORT_H */
