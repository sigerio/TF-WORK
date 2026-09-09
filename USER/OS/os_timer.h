/**
 * @file    os_timer.h
 * @brief   OS 定时任务管理 —— 周期软定时器
 *
 * ============ 特性 ============
 *
 * - 时间基准来源于 board_time.c 统一时间基准，底层由 HAL 实现
 * - 主循环读取时间基准，与 last_tick 比较判断到期并执行回调
 * - 无 ISR 参与，完全在主循环上下文中运行
 * - 仅支持周期任务，不支持单次
 * - 注册时 OS 内部按优先级（0~63）排序，高优先级靠前，同优先级按注册先后
 * - 回调可携带参数 void *arg
 * - 无 ISR 共享变量，Timer 系统完全在主循环上下文中运行
 *
 * ============ 使用示例 ============
 *
 * @code
 *   #include "OS/os.h"
 *
 *   void my_callback(void *arg) {
 *       (void)arg;
 *       PRINTF("Timer fired!\n");
 *   }
 *
 *   // 注册周期定时任务（初始化时调用）
 *   os_soft_timer_task(my_callback, NULL, 1000, 10);
 * @endcode
 */

#ifndef OS_TIMER_H
#define OS_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */

/** @brief 定时任务最大数量，可通过编译器 -D 覆盖 */
#ifndef OS_TIMER_MAX
#define OS_TIMER_MAX   16
#endif

/* ========== 对外接口 ========== */

/**
 * @brief 注册周期定时任务（仅初始化时调用）
 *
 * 将任务插入优先级排序的链表。同优先级按注册先后排列。
 * 注册完成后，主循环自动检查到期并执行回调。
 *
 * @param callback   回调函数指针
 * @param arg        回调参数（可为 NULL）
 * @param period_ms  周期（毫秒）
 * @param priority   优先级（0~63，0=最高，63=最低）
 */
void os_soft_timer_task(void (*callback)(void *), void *arg, uint32_t period_ms, uint8_t priority);

#ifdef __cplusplus
}
#endif

#endif /* OS_TIMER_H */
