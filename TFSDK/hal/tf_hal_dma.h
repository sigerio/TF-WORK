#ifndef TF_HAL_DMA_H
#define TF_HAL_DMA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct tf_drv_dma_config tf_drv_dma_config_t;

bool tf_hal_dma_init(const tf_drv_dma_config_t *cfg, uint8_t ch_index);
void tf_hal_dma_addr_config(void *base, uint8_t channel,
                            uint32_t periph_addr, uint32_t memory_addr);
void tf_hal_dma_number_config(void *base, uint8_t channel, uint32_t number);
void tf_hal_dma_start(void *base, uint8_t channel);
void tf_hal_dma_stop(void *base, uint8_t channel);
uint32_t tf_hal_dma_remaining_get(void *base, uint8_t channel);
void tf_hal_dma_interrupt_config(void *base, uint8_t channel, uint32_t int_mask);
uint32_t tf_hal_dma_flag_get(void *base, uint8_t channel, uint32_t flag);
void tf_hal_dma_flag_clear(void *base, uint8_t channel, uint32_t flag);

#endif /* TF_HAL_DMA_H */
