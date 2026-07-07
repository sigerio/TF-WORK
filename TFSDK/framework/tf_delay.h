/**
 * @file    tf_delay.h
 * @brief   统一时间管理 —— 延时 / 时间基准 / OS tick
 *
 * 本模块统一管理系统时间相关功能：
 * - 阻塞延时（ms/us 级，通过轮询时间基准实现）
 * - OS tick 源（tf_os_tick_get）
 * - 系统时钟频率管理
 *
 * ============ 架构 ============
 *
 * tf_delay.c 提供公共时间接口，HAL 层只提供底层 SysTick 配置函数：
 *
 *                  ┌─ tf_delay_ms / tf_delay_us
 *  用户 / OS 调用 ─┼─ tf_os_tick_get
 *                  ├─ tf_time_init / tf_time_ms
 *                  └─ tf_sysclk_freq_set / get
 *                               │
 *                               └─ tf_hal_systick_config
 *                                  （HAL 内部底层配置函数，非用户 API）
 *
 * ============ 使用示例 ============
 *
 * @code
 *   #include "tf_delay.h"
 *
 *   // 粗略延时（IRC8M 默认 8MHz，NOP 循环）
 *   tf_delay_ms(1);
 *
 *   // RCU 初始化后更新频率（delay 自动使用 SysTick 时间基准，更精确）
 *   tf_sysclk_freq_set(72000000);
 *
 *   // OS 主循环中获取 tick
 *   uint32_t now = tf_os_tick_get();
 * @endcode
 */

#ifndef TF_DELAY_H
#define TF_DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */

/** @brief 系统时钟频率（Hz），默认 IRC8M=8MHz，用户可 -D 覆盖 */
#ifndef TF_SYSCLK_FREQ
#define TF_SYSCLK_FREQ  8000000UL
#endif

/* ========== 延时接口（阻塞） ========== */

/**
 * @brief 阻塞延时（毫秒级）
 *
 * 由 delay 模块统一实现，HAL 层不覆盖。
 * - ms 级延时：轮询 SysTick 累计时间
 * - us < 1ms：基于 volatile NOP 循环粗略延时
 *
 * @param ms  延时毫秒数
 */
void tf_delay_ms(uint32_t ms);

/**
 * @brief 阻塞延时（微秒级）
 *
 * @param us  延时微秒数
 */
void tf_delay_us(uint32_t us);

/* ========== 时间基准接口 ========== */

/**
 * @brief 初始化时间基准
 *
 * 这是对外公开入口。内部会调用 HAL 层 SysTick 底层配置函数完成一次性初始化。
 * 可多次调用；重复调用不会重复配置硬件。
 */
void tf_time_init(void);

/**
 * @brief 获取自初始化以来的毫秒数
 *
 * 直接返回 SysTick 累计毫秒 tick。
 *
 * @return 毫秒数
 */
uint32_t tf_time_ms(void);

/**
 * @brief 设置运行时系统时钟频率
 *
 * 由 RCU 模块初始化完成后调用，更新延时精度。
 * 在时间基准初始化（SysTick 配置）前调用。
 *
 * @param hz  实际系统时钟频率（Hz）
 */
void tf_sysclk_freq_set(uint32_t hz);

/**
 * @brief 获取系统时钟频率
 *
 * @return 当前系统时钟频率（Hz）
 */
uint32_t tf_sysclk_freq_get(void);

#ifdef __cplusplus
}
#endif

#endif /* TF_DELAY_H */
