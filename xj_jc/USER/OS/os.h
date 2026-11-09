/**
 * @file    os.h
 * @brief   OS 层（系统调度器）—— 框架最高层
 *
 * OS 层拥有主循环，主循环中通过 os_tick_get() 读取统一时间基准，
 * 轮询 Timer/Event/Pool 在 main 超级循环中按序调度执行。
 *
 * ============ 主循环执行流程 ============
 *
 * @code
 *   while (1) {
 *       1. 遍历所有 Timer 节点：
 *          if (now - last_tick >= period_ms)
 *              执行 callback(arg)
 *              last_tick = now
 *
 *       2. 遍历所有 event_slots：
 *          if (count > 0)
 *              执行该事件下的所有订阅回调
 *              count--
 *
 *       3. 检查 Pool 队列非空：
 *          取出队头任务执行（仅一个）
 *
 *       4. 重复
 *   }
 * @endcode
 *
 * 执行顺序固定：Timer → Event → Pool，不可调整。
 *
 * ============ 启动流程 ============
 *
 * @code
 *   void main(void) {
 *       // 1. Port 层注册所有外设
 *       port_register_all();
 *       // 2. Module 初始化
 *       module_init_all();
 *       // 3. OS 初始化
 *       os_init();
 *       // 4. Application 初始化（注册 Timer/Event 任务）
 *       app_init();
 *       // 5. OS 接管主循环，永不返回
 *       os_main_loop();
 *   }
 * @endcode
 *
 * ============ 并发模型 ============
 *
 * | 任务  | 共享变量  | 生产者                | 消费者        | 保护策略               |
 * |-------|---------|-----------------------|--------------|----------------------|
 * | Timer | 无      | 读硬件定时器（无 ISR）  | 主循环只读     | 天然无竞态             |
 * | Event | count/arg | 业务代码             | 主循环自减/释放 | 用户保证无并发 trigger  |
 * | Pool  | head/tail| 主循环回调            | 主循环取出     | 单线程，无需保护        |
 *
 * ISR 调用限制：
 * - 禁止：os_soft_pool_task()（仅主循环内调用）
 * - 禁止：os_event_trigger()（可能分配 event 参数内存）
 * - 禁止：任何 Timer 操作（Timer 完全在主循环上下文中运行）
 * - 禁止：os_soft_pool_task()（仅主循环内调用）
 * - 禁止：任何 Timer 操作（Timer 完全在主循环上下文中运行）
 *
 * @see os_timer.h  定时任务管理
 * @see os_event.h  事件任务管理
 * @see os_pool.h   队列任务管理
 */

#ifndef OS_H
#define OS_H

#include <stdint.h>
#include "OS/os_timer.h"
#include "OS/os_event.h"
#include "OS/os_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 对外接口 ========== */

/**
 * @brief 初始化 OS
 *
 * - 清空所有任务池/事件槽/队列
 * - OS 层不配置任何硬件定时器——时间基准由 HAL 层通过 board_time.c 提供
 * - HAL 层需确保在 os_main_loop() 运行前，时间基准已可用
 */
void os_init(void);

/**
 * @brief OS 主循环（永不返回）
 *
 * 按固定顺序轮询：
 *   1. Timer 任务 — 检查到期并执行回调
 *   2. Event 任务 — 对有挂起事件的事件槽执行所有订阅回调
 *   3. Pool 任务 — 取队头一个任务执行
 *
 * 执行顺序固定：Timer → Event → Pool，不可调整。
 */
void os_main_loop(void);

/**
 * @brief 获取当前系统 tick（毫秒级）
 *
 * 时间来源由 board_time.c 统一管理，HAL 只提供底层 SysTick 配置函数。
 *
 * @return 当前 tick 值（从系统启动或参考点开始的毫秒数）
 */
uint32_t os_tick_get(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
