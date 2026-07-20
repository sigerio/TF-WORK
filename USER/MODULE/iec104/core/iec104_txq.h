#ifndef IEC104_TXQ_H
#define IEC104_TXQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void iec104_txq_init(void);
uint8_t iec104_txq_push(const uint8_t *apdu, uint16_t apdu_length);
uint8_t iec104_txq_remove_i_frames(void);
uint8_t iec104_txq_pop(uint8_t *apdu, uint16_t apdu_capacity, uint16_t *apdu_length);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_TXQ_H */
