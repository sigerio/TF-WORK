#ifndef IEC104_PORT_H
#define IEC104_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IEC104 传输端口接口。
 *
 * 门面层只通过本接口搬运完整 APDU，不依赖具体外设。移植到其他链路时，由新的
 * iec104_port_xxx 实现提供同名函数。
 */

/* 发送侧：门面先取得缓冲区并填入 APDU，再启动一次异步发送。 */
uint8_t *iec104_port_tx_buffer(void);
uint8_t iec104_port_tx_busy(void);
uint8_t iec104_port_tx_start(uint16_t apdu_length);

/* 轮询异步发送完成状态，完成后释放发送缓冲区。 */
void iec104_port_tx_service(void);

/* 接收侧：available 为 FIFO 字节数，peek 不消费，read 成功后消费。 */
uint16_t iec104_port_rx_available(void);
uint8_t iec104_port_rx_peek(uint8_t *data, uint16_t length);
uint8_t iec104_port_rx_read(uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_PORT_H */
