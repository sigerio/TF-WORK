/**
 * @file    tf_os_event.c
 * @brief   OS 事件任务管理实现
 *
 * 维护事件 pool + 调度 slot。pool 按注册顺序保存事件实体，
 * slot 按事件优先级保存 pool 索引，trigger 时通过 event_id 映射
 * 找到对应 pool 项并更新参数副本。
 */

#include "tf_os_event.h"

#include <string.h>
 
#include "../tf_debug.h"
#include "../../../USER/LIB/heap_4.h"

/* ========== 内部数据结构 ========== */

typedef struct {
    void     (*callback)(void *);   /**< 回调函数指针           */
    uint8_t    event_id;            /**< 关联事件 ID            */
    uint8_t    priority;            /**< 优先级（0~63）         */
    volatile uint8_t count;         /**< >0=忙碌, ==0=空闲      */
    void            *arg;           /**< trigger 参数副本       */
    uint16_t         arg_size;      /**< trigger 参数大小       */
} tf_os_event_pool_t;

typedef struct {
    int8_t pool_index;              /**< 指向 os_event_pool[]   */
} tf_os_event_slot_t;

/* ========== 静态数据 ========== */

/** @brief 事件实体池：按注册顺序 append，不插入、不搬移 */
static tf_os_event_pool_t os_event_pool[TF_OS_EVENT_NODE_MAX];

/** @brief 调度槽：按 priority/pool index 排序，保存 pool 索引 */
static tf_os_event_slot_t os_event_slots[TF_OS_EVENT_NODE_MAX];

/** @brief event_id 到 pool 索引的映射，-1 表示未注册 */
static int8_t os_event_id_map[TF_OS_EVENT_SLOT_MAX];

/** @brief 已注册事件计数 */
static uint8_t event_pool_count = 0;

/** @brief 调度槽计数 */
static uint8_t event_slot_count = 0;

/** @brief event 参数内存池 */
static uint8_t os_event_heap_buf[TF_OS_EVENT_HEAP_SIZE];

/** @brief event 参数内存池句柄 */
static HeapHandle_t os_event_heap = portHEAP_INVALID_HANDLE;

static uint8_t tf_os_event_pool_before(uint8_t lhs_pool,
                                       uint8_t rhs_pool)
{
    const tf_os_event_pool_t *lhs = &os_event_pool[lhs_pool];
    const tf_os_event_pool_t *rhs = &os_event_pool[rhs_pool];

    if (lhs->priority < rhs->priority) {
        return 1U;
    }

    if (lhs->priority > rhs->priority) {
        return 0U;
    }

    return (lhs_pool < rhs_pool) ? 1U : 0U;
}

static uint8_t tf_os_event_find_slot_insert_pos(uint8_t pool_index)
{
    uint8_t i;

    for (i = 0U; i < event_slot_count; i++) {
        if (tf_os_event_pool_before(pool_index,
                                    (uint8_t)os_event_slots[i].pool_index)) {
            break;
        }
    }

    return i;
}

static void tf_os_event_slot_shift_right(uint8_t pos)
{
    uint8_t i = event_slot_count;

    while (i > pos) {
        os_event_slots[i] = os_event_slots[i - 1U];
        i--;
    }
}

/* ========== 对外接口实现 ========== */

void tf_os_soft_event_task(uint8_t event_id, void (*callback)(void *),
                           uint8_t priority)
{
    if (event_id >= TF_OS_EVENT_SLOT_MAX) {
        TF_DEBUG("EVENT", "soft_event_task: event_id=%u >= SLOT_MAX=%u\n",
                 (unsigned)event_id, (unsigned)TF_OS_EVENT_SLOT_MAX);
        return;
    }

    if (callback == NULL) {
        TF_DEBUG("EVENT", "soft_event_task: ev=%u cb=NULL\n",
                 (unsigned)event_id);
        return;
    }

    if (os_event_id_map[event_id] >= 0) {
        TF_DEBUG("EVENT", "soft_event_task: ev=%u already registered\n",
                 (unsigned)event_id);
        return;
    }

    if (event_pool_count >= TF_OS_EVENT_NODE_MAX) {
        TF_DEBUG("EVENT", "soft_event_task: node pool full (%u/%u)\n",
                 (unsigned)event_pool_count, (unsigned)TF_OS_EVENT_NODE_MAX);
        return;
    }

    uint8_t pool_index = event_pool_count;
    uint8_t slot_pos;
    tf_os_event_pool_t *event = &os_event_pool[pool_index];

    event->callback = callback;
    event->event_id = event_id;
    event->priority = priority;
    event->count    = 0U;
    event->arg      = NULL;
    event->arg_size = 0U;

    os_event_id_map[event_id] = (int8_t)pool_index;
    event_pool_count++;

    slot_pos = tf_os_event_find_slot_insert_pos(pool_index);
    tf_os_event_slot_shift_right(slot_pos);
    os_event_slots[slot_pos].pool_index = (int8_t)pool_index;
    event_slot_count++;

    TF_DEBUG("EVENT", "soft_event_task: ev=%u pool=%u slot=%u cb=%p pri=%u "
             "total=%u\n",
             (unsigned)event_id, (unsigned)pool_index, (unsigned)slot_pos,
             (void *)callback, (unsigned)priority,
             (unsigned)event_pool_count);
}

int8_t tf_os_event_trigger(uint8_t event_id,
                           const void *arg,
                           uint16_t arg_size,
                           uint8_t count)
{
    tf_os_event_pool_t *event;
    int8_t pool_index;
    void *event_arg = NULL;

    if (event_id >= TF_OS_EVENT_SLOT_MAX) {
        return -1;
    }

    if (count == 0U) {
        return -2;
    }

    pool_index = os_event_id_map[event_id];
    if (pool_index < 0) {
        return -5;
    }

    event = &os_event_pool[pool_index];

    if ((arg != NULL) && (arg_size > 0U)) {
        if (os_event_heap == portHEAP_INVALID_HANDLE) {
            return -3;
        }

        event_arg = pvPortMalloc(os_event_heap, arg_size);
        if (event_arg == NULL) {
            TF_DEBUG("EVENT", "trigger: ev=%u malloc %u failed\n",
                     (unsigned)event_id, (unsigned)arg_size);
            return -4;
        }

        memcpy(event_arg, arg, arg_size);
    }

    if (event->arg != NULL) {
        vPortFree(os_event_heap, event->arg);
    }

    event->arg = event_arg;
    event->arg_size = (event_arg != NULL) ? arg_size : 0U;

    if (event->count == 0U) {
        event->count = count;
    }

    TF_DEBUG("EVENT", "trigger: ev=%u pool=%d count=%u arg=%p size=%u\n",
             (unsigned)event_id, pool_index, (unsigned)event->count,
             event->arg, (unsigned)event->arg_size);

    return 0;
}

/* ========== 内部接口（主循环调用） ========== */

/**
 * @brief 初始化事件系统（由 tf_os_init 调用）
 */
void tf_os_event_init(void)
{
    event_pool_count = 0U;
    event_slot_count = 0U;
    os_event_heap = vPortHeapInit(os_event_heap_buf,
                                  sizeof(os_event_heap_buf));

    for (uint8_t i = 0; i < TF_OS_EVENT_SLOT_MAX; i++) {
        os_event_id_map[i] = -1;
    }

    for (uint8_t i = 0; i < TF_OS_EVENT_NODE_MAX; i++) {
        os_event_pool[i].callback = NULL;
        os_event_pool[i].event_id = 0U;
        os_event_pool[i].priority = 0U;
        os_event_pool[i].count    = 0U;
        os_event_pool[i].arg      = NULL;
        os_event_pool[i].arg_size = 0U;

        os_event_slots[i].pool_index = -1;
    }
}

/**
 * @brief 执行一轮事件处理（由 tf_os_main_loop 调用）
 */
void tf_os_event_process(void)
{
    for (uint8_t i = 0; i < event_slot_count; i++) {
        int8_t pool_index = os_event_slots[i].pool_index;
        tf_os_event_pool_t *event;
        void *arg;

        if (pool_index < 0) {
            continue;
        }

        event = &os_event_pool[pool_index];
        if (event->count == 0U) {
            continue;
        }

        arg = event->arg;

        TF_DEBUG("EVENT", "process: slot=%u pool=%d ev=%u count=%u\n",
                 (unsigned)i, pool_index, (unsigned)event->event_id,
                 (unsigned)event->count);

        if (event->callback != NULL) {
            event->callback(arg);
        }

        event->count--;
        if (event->count == 0U) {
            if (event->arg != NULL) {
                vPortFree(os_event_heap, event->arg);
                event->arg = NULL;
                event->arg_size = 0U;
            }
        }
    }
}
