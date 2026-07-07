/**
 * @file    tf_os_pool.h
 * @brief   OS 队列任务管理 —— FIFO 任务队列
 *
 * ============ 特性 ============
 *
 * - 仅主循环内使用（Application 回调中动态添加），ISR 中禁止调用
 * - 全局一个 FIFO 队列，无优先级，count 管理满/空
 * - 每次主循环仅执行队头一个任务，执行后从队列移除
 * - 满时（count==MAX）：返回 -1，静默丢弃
 * - 空时（count==0）：主循环跳过
 * - 单线程操作，无需并发保护
 *
 * ============ 使用示例 ============
 *
 * @code
 *   #include "os/tf_os.h"
 *
 *   void deferred_work(void *arg) {
 *       TF_PRINTF("Deferred task: %s\n", (const char *)arg);
 *   }
 *
 *   // 在主循环回调中动态添加池任务
 *   int8_t ret = tf_os_soft_pool_task(deferred_work, "hello");
 *   if (ret == -1) {
 *       TF_PRINTF("Pool full!\n");
 *   }
 * @endcode
 */

#ifndef TF_OS_POOL_H
#define TF_OS_POOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */

/** @brief 队列最大容量，可通过编译器 -D 覆盖 */
#ifndef TF_OS_POOL_MAX
#define TF_OS_POOL_MAX   8
#endif

/* ========== 对外接口 ========== */

/**
 * @brief 添加任务到队列（仅主循环内调用，ISR 中禁止）
 *
 * 将回调函数+参数推入 FIFO 队列。满时静默丢弃。
 *
 * @param callback  回调函数指针
 * @param arg       回调参数（可为 NULL）
 * @return          0=成功, -1=队列满
 */
int8_t tf_os_soft_pool_task(void (*callback)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif /* TF_OS_POOL_H */
