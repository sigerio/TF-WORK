/**
 * @file    tf_os_pool.h
 * @brief   OS 队列任务管理 —— FIFO 任务队列
 *
 * ============ 特性 ============
 *
 * - 仅主循环内使用（Application 回调中动态添加），ISR 中禁止调用
 * - 全局一个 FIFO 队列，无优先级，count 管理满/空
 * - 每次主循环仅执行队头一个任务，执行后从队列移除
 * - arg 非空且 arg_size > 0 时，内部从 pool heap 分配并拷贝参数
 * - callback 消费的是内部参数副本，callback 返回后由 Pool 释放
 * - 满时（count==MAX）：返回负数，任务不入队
 * - 空时（count==0）：主循环跳过
 * - 单线程操作，无需并发保护
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

#if ((TF_OS_POOL_MAX == 0U) || (TF_OS_POOL_MAX > 255U))
#error TF_OS_POOL_MAX must be in the range 1..255
#endif

/** @brief Pool 参数内存池大小（字节），可通过编译器 -D 覆盖 */
#ifndef TF_OS_POOL_HEAP_SIZE
#define TF_OS_POOL_HEAP_SIZE   256U
#endif

#define TF_OS_POOL_OK                 0
#define TF_OS_POOL_ERR_FULL           (-1)
#define TF_OS_POOL_ERR_CALLBACK       (-2)
#define TF_OS_POOL_ERR_HEAP           (-3)
#define TF_OS_POOL_ERR_ALLOC          (-4)
#define TF_OS_POOL_ERR_EMPTY          (-5)

/* ========== 对外接口 ========== */

/**
 * @brief 添加任务到队列（仅主循环内调用，ISR 中禁止）
 *
 * 将回调函数和参数副本推入 FIFO 队列。
 *
 * @param callback  回调函数指针
 * @param arg       回调参数源地址（可为 NULL）
 * @param arg_size  回调参数字节数，arg 为 NULL 或 arg_size 为 0 时不拷贝参数
 * @return          0=成功，负数=失败
 */
int8_t tf_os_soft_pool_task(void (*callback)(void *),
                            const void *arg,
                            uint16_t arg_size);

#ifdef __cplusplus
}
#endif

#endif /* TF_OS_POOL_H */
