#ifndef IEC104_TXQ_H
#define IEC104_TXQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 尚未交给物理端口的 APDU 发送队列。 */
void iec104_txq_init(void);

/* 检查完整 APDU 及队列长度前缀是否能入队，不占用空间。 */
uint8_t iec104_txq_can_push(uint16_t apdu_length);

/* 入队成功返回 1，队列空间不足返回 0。 */
uint8_t iec104_txq_push(const uint8_t *apdu, uint16_t apdu_length);

/* 出队成功返回 1，队列为空或目标容量不足返回 0。 */
uint8_t iec104_txq_pop(uint8_t *apdu, uint16_t apdu_capacity, uint16_t *apdu_length);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_TXQ_H */
