/**
 * @file    board_time.h
 * @brief   统一时间管理 —— 延时 / 时间基准 / OS tick
 *
 * 本模块提供阻塞延时和 OS 的毫秒 tick。
 */

#ifndef BOARD_TIME_H
#define BOARD_TIME_H

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
void board_delay_ms(uint32_t ms);

/**
 * @brief 阻塞延时（微秒级）
 *
 * @param us  延时微秒数
 */
void board_delay_us(uint32_t us);

/* ========== 时间基准接口 ========== */

/**
 * @brief 初始化时间基准
 *
 * 直接配置 1ms SysTick 时间基准。
 * 由启动阶段的 main() 调用一次。
 */
void board_time_init(void);

/**
 * @brief 获取自初始化以来的毫秒数
 *
 * 直接返回 SysTick 累计毫秒 tick。
 *
 * @return 毫秒数
 */
uint32_t board_time_ms(void);

/* 由 SysTick_Handler 转发，每次中断累加一个毫秒 tick。 */
void board_time_systick_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_TIME_H */
