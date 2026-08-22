#ifndef IEC104_PORT_H
#define IEC104_PORT_H

#include <stdint.h>

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IEC104 传输端口接口。
 *
 * 门面层只通过本接口搬运完整 APDU，不依赖具体外设。
 * BOARD_USART2_ROLE 为 IEC104 时由 Windows 上位机经 USART2 直接连接本机；
 * 其他角色时 IEC104 经 USART1 连接主控板。协议栈和业务处理保持不变。
 */

uint8_t iec104_port_init(void);
void iec104_port_irq(void);
void iec104_port_rx_dma_irq(void);

/* 发送侧：门面先取得缓冲区并填入 APDU，再启动一次异步发送。 */
uint8_t *iec104_port_tx_buffer(void);
uint8_t iec104_port_tx_busy(void);
uint8_t iec104_port_tx_start(uint16_t apdu_length);

/* 轮询异步发送完成状态，完成后释放发送缓冲区。 */
void iec104_port_tx_service(void);

/* available 为 FIFO 字节数；peek 不消费；read 仅在完整读取时消费。 */
uint16_t iec104_port_rx_available(void);
uint8_t iec104_port_rx_peek(uint8_t *data, uint16_t length);
uint8_t iec104_port_rx_read(uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_PORT_H */
