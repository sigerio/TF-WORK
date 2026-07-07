/**
 * @file    tf_os_pool.c
 * @brief   OS 队列任务管理实现
 *
 * 使用静态循环队列实现 FIFO 任务队列。单线程操作，
 * ISR 禁止调用，无需并发保护。
 */

#include "tf_os_pool.h"
 
#include "../tf_debug.h"

/* ========== 内部数据结构 ========== */

typedef struct {
    void (*callback)(void *);   /**< 回调函数指针 */
    void  *arg;                 /**< 回调参数     */
} tf_os_pool_item_t;

/* ========== 静态数据 ========== */

/** @brief 循环队列 */
static tf_os_pool_item_t os_pool_queue[TF_OS_POOL_MAX];

/** @brief 队头索引（出队位置） */
static uint8_t os_pool_head = 0;

/** @brief 队尾索引（入队位置） */
static uint8_t os_pool_tail = 0;

/** @brief 队列中当前元素数量 */
static uint8_t os_pool_count = 0;

/* ========== 对外接口实现 ========== */

int8_t tf_os_soft_pool_task(void (*callback)(void *), void *arg)
{
    if (os_pool_count >= TF_OS_POOL_MAX) {
        TF_DEBUG("OS", "soft_pool_task: queue full (%u/%u)\n",
                 (unsigned)os_pool_count, (unsigned)TF_OS_POOL_MAX);
        return -1;
    }

    os_pool_queue[os_pool_tail].callback = callback;
    os_pool_queue[os_pool_tail].arg      = arg;
    os_pool_tail = (os_pool_tail + 1) % TF_OS_POOL_MAX;
    os_pool_count++;

    TF_DEBUG("OS", "soft_pool_task: cb=%p arg=%p count=%u\n",
             (void *)callback, arg, (unsigned)os_pool_count);

    return 0;
}

/* ========== 内部接口（主循环调用） ========== */

/**
 * @brief 初始化 Pool 队列（由 tf_os_init 调用）
 */
void tf_os_pool_init(void)
{
    os_pool_head  = 0;
    os_pool_tail  = 0;
    os_pool_count = 0;
}

/**
 * @brief 执行队头一个池任务（由 tf_os_main_loop 调用）
 *
 * @return 0=有任务被执行, -1=队列空
 */
int8_t tf_os_pool_process(void)
{
    if (os_pool_count == 0) return -1;

    tf_os_pool_item_t *item = &os_pool_queue[os_pool_head];
    os_pool_head = (os_pool_head + 1) % TF_OS_POOL_MAX;
    os_pool_count--;

    TF_DEBUG("OS", "pool_process: cb=%p arg=%p remaining=%u\n",
             (void *)item->callback, item->arg, (unsigned)os_pool_count);

    item->callback(item->arg);
    return 0;
}
