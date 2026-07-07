#ifndef TF_HAL_ADC_H
#define TF_HAL_ADC_H

#include <stdint.h>

/* ========== 初始化（参数由 HAL 内部 per-instance 回调提供）========== */
void tf_hal_adc_init(void *base);

/* ========== 校准 ========== */
void tf_hal_adc_calibration_enable(void *base);

/* ========== 运行时操作 ========== */
void tf_hal_adc_software_trigger_enable(void *base, uint8_t group);
uint16_t tf_hal_adc_regular_data_read(void *base);
uint16_t tf_hal_adc_inserted_data_read(void *base, uint8_t channel);
uint32_t tf_hal_adc_flag_get(void *base, uint32_t flag);
void tf_hal_adc_flag_clear(void *base, uint32_t flag);

/* ========== 平台层 IRQ 接入 ========== */
void tf_hal_adc_interrupt_enable(void *base, uint32_t int_mask);
void tf_hal_adc_interrupt_disable(void *base, uint32_t int_mask);

#endif /* TF_HAL_ADC_H */
