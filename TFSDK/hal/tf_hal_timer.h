#ifndef TF_HAL_TIMER_H
#define TF_HAL_TIMER_H

#include <stdint.h>

typedef struct tf_drv_timer_config tf_drv_timer_config_t;

/* HAL 层通过 config 指针获取硬件参数（prescaler/period 等） */
void tf_hal_timer_init(const tf_drv_timer_config_t *cfg);
void tf_hal_timer_start(void *tim);
void tf_hal_timer_stop(void *tim);
uint32_t tf_hal_timer_get_cnt(void *tim);
/* 使能/禁能指定通道输出（通道号由 board config 提供） */
void tf_hal_timer_ch_output_enable(void *tim, uint32_t channel);
void tf_hal_timer_ch_output_disable(void *tim, uint32_t channel);
/* 配置通道 PWM 模式（通道号由 board config 提供） */
void tf_hal_timer_ch_pwm_mode_config(void *tim, uint32_t channel);
/* 运行时设置占空比百分比（0~100），HAL 内部换算为 CCR */
void tf_hal_timer_ch_pwm_set_duty(void *tim, uint32_t channel, uint8_t duty);
/* 直接设置 CCR（需要 HAL 知道 period 的场景内部处理） */
void tf_hal_timer_ch_set_ccr(void *tim, uint32_t channel, uint32_t ccr);
void tf_hal_timer_interrupt_enable(void *tim, uint32_t int_mask);
void tf_hal_timer_interrupt_disable(void *tim, uint32_t int_mask);
uint32_t tf_hal_timer_flag_get(void *tim, uint32_t flag);
void tf_hal_timer_flag_clear(void *tim, uint32_t flag);
void tf_hal_timer_deinit(void *tim);

#endif /* TF_HAL_TIMER_H */
