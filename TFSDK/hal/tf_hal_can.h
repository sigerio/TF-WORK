#ifndef TF_HAL_CAN_H
#define TF_HAL_CAN_H

#include <stdint.h>

/* ========== 初始化（参数由 HAL 内部 per-instance 回调提供）========== */
void tf_hal_can_init(void *base);

/* ========== 运行时操作 ========== */
uint8_t tf_hal_can_message_transmit(void *base, uint32_t tmi, uint32_t tmp,
                                    uint32_t tmdata0, uint32_t tmdata1);
uint32_t tf_hal_can_transmit_states(void *base, uint8_t mailbox);
void tf_hal_can_transmission_stop(void *base, uint8_t mailbox);
void tf_hal_can_message_receive(void *base, uint8_t fifo,
                                uint32_t *mi, uint32_t *mp,
                                uint32_t *mdata0, uint32_t *mdata1);
void tf_hal_can_fifo_release(void *base, uint8_t fifo);
uint8_t tf_hal_can_receive_message_length_get(void *base, uint8_t fifo);

/* ========== 标志位 ========== */
uint32_t tf_hal_can_flag_get(void *base, uint32_t flag);
void tf_hal_can_flag_clear(void *base, uint32_t flag);

/* ========== 平台层 IRQ 接入 ========== */
void tf_hal_can_interrupt_enable(void *base, uint32_t int_mask);
void tf_hal_can_interrupt_disable(void *base, uint32_t int_mask);

#endif /* TF_HAL_CAN_H */
