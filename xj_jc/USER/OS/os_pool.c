/**
 * @file    os_pool.c
 * @brief   OS 队列任务管理实现
 *
 * 使用静态循环队列实现 FIFO 任务队列。单线程操作，
 * ISR 禁止调用，无需并发保护。
 */

#include "OS/os_pool.h"
#include "OS/os_internal.h"

#include <string.h>

#include "DEBUG/debug_log.h"
#include "COMMON/heap_4.h"

/* ========== 内部数据结构 ========== */

typedef struct {
    void (*callback)(void *);   /**< 回调函数指针 */
    void  *arg;                 /**< 回调参数     */
    uint16_t arg_size;          /**< 回调参数大小 */
} os_pool_item_t;

/* ========== 静态数据 ========== */

/** @brief 循环队列 */
static os_pool_item_t os_pool_queue[OS_POOL_MAX];

/** @brief 队头索引（出队位置） */
static uint8_t os_pool_head = 0;

/** @brief 队尾索引（入队位置） */
static uint8_t os_pool_tail = 0;

/** @brief 队列中当前元素数量 */
static uint8_t os_pool_count = 0;

/** @brief Pool 参数内存池 */
static uint8_t os_pool_heap_buf[OS_POOL_HEAP_SIZE];

/** @brief Pool 参数内存池句柄 */
static HeapHandle_t os_pool_heap = portHEAP_INVALID_HANDLE;

/* ========== 对外接口实现 ========== */

int8_t os_soft_pool_task(void (*callback)(void *), const void *arg, uint16_t arg_size)
{
    void *pool_arg = 0;

    if (callback == 0) {
        DEBUG("OS", "soft_pool_task: cb=NULL\n");
        return OS_POOL_ERR_CALLBACK;
    }

    if (os_pool_count >= OS_POOL_MAX) {
        DEBUG("OS", "soft_pool_task: queue full (%u/%u)\n", (unsigned)os_pool_count, (unsigned)OS_POOL_MAX);
        return OS_POOL_ERR_FULL;
    }

    if ((arg != 0) && (arg_size > 0U)) {
        if (os_pool_heap == portHEAP_INVALID_HANDLE) {
            return OS_POOL_ERR_HEAP;
        }

        pool_arg = pvPortMalloc(os_pool_heap, arg_size);
        if (pool_arg == 0) {
            DEBUG("OS", "soft_pool_task: malloc %u failed\n", (unsigned)arg_size);
            return OS_POOL_ERR_ALLOC;
        }

        memcpy(pool_arg, arg, arg_size);
    }

    os_pool_queue[os_pool_tail].callback = callback;
    os_pool_queue[os_pool_tail].arg      = pool_arg;
    os_pool_queue[os_pool_tail].arg_size = (pool_arg != 0) ? arg_size : 0U;
    os_pool_tail = (os_pool_tail + 1) & OS_POOL_MASK;
    os_pool_count++;

    DEBUG("OS", "soft_pool_task: cb=%p arg=%p size=%u count=%u\n", (void *)callback, pool_arg, (unsigned)arg_size, (unsigned)os_pool_count);

    return OS_POOL_OK;
}

/* ========== 内部接口（主循环调用） ========== */

/**
 * @brief 初始化 Pool 队列（由 os_init 调用）
 */
void os_pool_init(void)
{
    uint8_t i;

    os_pool_head  = 0;
    os_pool_tail  = 0;
    os_pool_count = 0;
    os_pool_heap = vPortHeapInit(os_pool_heap_buf, sizeof(os_pool_heap_buf));

    for (i = 0U; i < OS_POOL_MAX; i++) {
        os_pool_queue[i].callback = 0;
        os_pool_queue[i].arg = 0;
        os_pool_queue[i].arg_size = 0U;
    }
}

/**
 * @brief 执行队头一个池任务（由 os_main_loop 调用）
 *
 * @return 0=有任务被执行，负数=失败或队列空
 */
int8_t os_pool_process(void)
{
    os_pool_item_t item;

    if (os_pool_count == 0) {
        return OS_POOL_ERR_EMPTY;
    }

    item = os_pool_queue[os_pool_head];
    os_pool_queue[os_pool_head].callback = 0;
    os_pool_queue[os_pool_head].arg = 0;
    os_pool_queue[os_pool_head].arg_size = 0U;
    os_pool_head = (os_pool_head + 1) & OS_POOL_MASK;
    os_pool_count--;

    DEBUG("OS", "pool_process: cb=%p arg=%p size=%u remaining=%u\n", (void *)item.callback,
        item.arg, (unsigned)item.arg_size, (unsigned)os_pool_count);

    if (item.callback != 0) {
        item.callback(item.arg);
    }

    if (item.arg != 0) {
        vPortFree(os_pool_heap, item.arg);
    }
    return OS_POOL_OK;
}
