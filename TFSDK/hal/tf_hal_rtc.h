#ifndef TF_HAL_RTC_H
#define TF_HAL_RTC_H

#include <stdint.h>

/* ========== 初始化（参数由 HAL 内部硬编码）========== */
void tf_hal_rtc_init(void);

/* ========== 运行时操作 ========== */
void tf_hal_rtc_counter_set(uint32_t cnt);
uint32_t tf_hal_rtc_counter_get(void);
void tf_hal_rtc_prescaler_set(uint32_t psc);
void tf_hal_rtc_alarm_config(uint32_t alarm);

/* ========== 标志位 ========== */
uint32_t tf_hal_rtc_flag_get(uint32_t flag);
void tf_hal_rtc_flag_clear(uint32_t flag);
uint32_t tf_hal_rtc_interrupt_flag_get(uint32_t flag);
void tf_hal_rtc_interrupt_flag_clear(uint32_t flag);

/* ========== 平台层 IRQ 接入 ========== */
void tf_hal_rtc_interrupt_enable(uint32_t interrupt);
void tf_hal_rtc_interrupt_disable(uint32_t interrupt);

#endif /* TF_HAL_RTC_H */
