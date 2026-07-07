/**
 * @file    tf_drv_systick.h
 * @brief   SysTick 系统滴答定时器驱动接口
 */

#ifndef TF_DRV_SYSTICK_H
#define TF_DRV_SYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *tf_drv_systick_handle_t;

/**
 * @brief 注册并启动 SysTick 时间基准
 *
 * SysTick 为内核外设，无外设 ID，无 config 结构体，单实例。
 *
 * @return SysTick 句柄
 */
tf_drv_systick_handle_t tf_drv_systick_register(void);

/**
 * @brief 获取 SysTick 句柄
 *
 * @return 已注册返回句柄，未注册返回 NULL
 */
tf_drv_systick_handle_t tf_drv_systick_get_handle(void);

/**
 * @brief 检查 SysTick 是否已注册
 *
 * @return true=已注册, false=未注册
 */
bool tf_drv_systick_is_registered(void);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_SYSTICK_H */
