/**
 * @file    tf_drv_timer.h
 * @brief   Timer 外设注册与通道操作
 *
 * Timer 在 TF SDK 中建模为“父设备 + 内部子通道”：
 * - 通道静态配置放在 timer_configs[].ch[]
 * - `tf_drv_timer_register()` 内部完成父定时器初始化与通道初始化
 * - 公共层不再暴露通道配置/注册步骤
 */

#ifndef TF_DRV_TIMER_H
#define TF_DRV_TIMER_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
 



#ifdef __cplusplus
extern "C" {
#endif

/* ========== 句柄类型 ========== */

typedef void * tf_drv_timer_handle_t;


/* ========== 配置结构体 ========== */

typedef struct {
    int     io_id;          /**< 通道关联 GPIO 资源 ID (E_IO_xxx) */
    uint32_t channel;       /**< 平台原生硬件通道号（如 TIM_CHANNEL_1） */
    bool    enabled;        /**< true=启用该通道                   */
} tf_drv_timer_ch_cfg_t;

typedef struct tf_drv_timer_config {
    void     *tim;           /**< 硬件基地址                        */
    uint16_t  prescaler;     /**< 预分频值（PSC）                   */
    uint32_t  period;        /**< 自动重载值（ARR）                 */
    uint8_t   channel_count; /**< 实例实际通道数；0 表示使用最大容量 */
    /* ---- 通道子资源 ---- */
    tf_drv_timer_ch_cfg_t ch[E_DRV_T2_CH_MAX];
} tf_drv_timer_config_t;

#if MCU_CONTAIN_PRE_TIM
extern tf_drv_timer_config_t timer_configs[];
#endif

/* ========== 物理定时器操作（驱动）========== */

/**
 * @brief 注册物理定时器
 *
 * @param id   Timer 枚举 ID
 * @return     句柄，失败返回 NULL
 */
tf_drv_timer_handle_t tf_drv_timer_register(int id);

/**
 * @brief 启动定时器计数
 */
void tf_drv_timer_start(tf_drv_timer_handle_t h);

/**
 * @brief 停止定时器计数
 */
void tf_drv_timer_stop(tf_drv_timer_handle_t h);

/**
 * @brief 根据枚举 ID 获取句柄
 */
tf_drv_timer_handle_t tf_drv_timer_get_handle(int id);

uint32_t tf_drv_timer_get_tick(tf_drv_timer_handle_t h);
void tf_drv_timer_ch_start(tf_drv_timer_handle_t h, uint32_t ch);
void tf_drv_timer_ch_stop(tf_drv_timer_handle_t h, uint32_t ch);
void tf_drv_timer_ch_pwm_set_duty(tf_drv_timer_handle_t h, uint32_t ch, uint8_t duty);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_TIMER_H */
