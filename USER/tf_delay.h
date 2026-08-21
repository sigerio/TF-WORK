/**
 * @file    tf_delay.h
 * @brief   统一时间管理 —— 延时 / 时间基准 / OS tick
 *
 * 本模块提供阻塞延时和 OS 的毫秒 tick。
 */

#ifndef TF_DELAY_H
#define TF_DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 延时接口（阻塞） ========== */

/**
 * @brief 阻塞延时（毫秒级）
 *
 * 由 delay 模块统一实现。
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
 * 直接配置 1ms SysTick 时间基准。
 * 由启动阶段的 main() 调用一次。
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

#ifdef __cplusplus
}
#endif

#endif /* TF_DELAY_H */
