/**
 * @file    tf_os_event.c
 * @brief   OS 事件任务管理实现
 *
 * 维护事件槽数组 + 回调节点池。每个槽位维护一个优先级排序的
 * 回调链表，和 volatile count 状态变量（ISR/主循环共享）。
 */

#include "tf_os_event.h"
 
#include "../tf_debug.h"

/* ========== 内部数据结构 ========== */

/**
 * @brief 事件回调节点
 *
 * 仅 init 时注册，运行时只读。
 */
typedef struct {
    void     (*callback)(void *);   /**< 回调函数指针           */
    void      *arg;                 /**< 回调参数               */
    uint8_t    event_id;            /**< 关联事件 ID            */
    uint8_t    priority;            /**< 优先级（0~63）         */
    int8_t     next;                /**< 链表下一节点索引，-1=尾 */
} tf_os_event_node_t;

/**
 * @brief 事件槽
 *
 * count 为 volatile 状态：ISR 写入（仅 count==0），主循环读写（自减）。
 * callback_head 仅 init 时写入，运行时只读，无需 volatile。
 */
typedef struct {
    volatile uint8_t count;         /**< >0=忙碌, ==0=空闲      */
    int8_t           callback_head; /**< 该事件回调链表头       */
} tf_os_event_slot_t;

/* ========== 静态数据 ========== */

/** @brief 事件回调节点池 */
static tf_os_event_node_t os_event_pool[TF_OS_EVENT_NODE_MAX];

/** @brief 事件槽数组 */
static tf_os_event_slot_t os_event_slots[TF_OS_EVENT_SLOT_MAX];

/** @brief 已注册节点计数 */
static uint8_t event_node_count = 0;

/* ========== 对外接口实现 ========== */

void tf_os_soft_event_task(uint8_t event_id, void (*callback)(void *),
                           void *arg, uint8_t priority)
{
    if (event_id >= TF_OS_EVENT_SLOT_MAX) {
        TF_DEBUG("EVENT", "soft_event_task: event_id=%u >= SLOT_MAX=%u\n",
                 (unsigned)event_id, (unsigned)TF_OS_EVENT_SLOT_MAX);
        return;
    }

    if (event_node_count >= TF_OS_EVENT_NODE_MAX) {
        TF_DEBUG("EVENT", "soft_event_task: node pool full (%u/%u)\n",
                 (unsigned)event_node_count, (unsigned)TF_OS_EVENT_NODE_MAX);
        return;
    }

    /* 分配新节点 */
    int8_t node_idx = (int8_t)event_node_count;
    tf_os_event_node_t *node = &os_event_pool[node_idx];

    node->callback = callback;
    node->arg      = arg;
    node->event_id = event_id;
    node->priority = priority;
    node->next     = -1;

    /* 按优先级插入该事件槽的回调链表 */
    int8_t *prev = &os_event_slots[event_id].callback_head;
    int8_t  curr = os_event_slots[event_id].callback_head;

    while (curr >= 0) {
        if (os_event_pool[curr].priority > priority) {
            break;
        }
        prev = &os_event_pool[curr].next;
        curr = os_event_pool[curr].next;
    }

    node->next = curr;
    *prev = node_idx;
    event_node_count++;

    TF_DEBUG("EVENT", "soft_event_task: ev=%u node=%d cb=%p pri=%u "
             "total=%u\n",
             (unsigned)event_id, node_idx, (void *)callback,
             (unsigned)priority, (unsigned)event_node_count);
}

void tf_os_event_trigger(uint8_t event_id, uint8_t count)
{
    if (event_id >= TF_OS_EVENT_SLOT_MAX) return;
    if (count == 0) return;

    /* 仅当 count==0（空闲）时写入，忙碌时静默丢弃 */
    if (os_event_slots[event_id].count == 0) {
        os_event_slots[event_id].count = count;
        TF_DEBUG("EVENT", "trigger: ev=%u count=%u\n",
                 (unsigned)event_id, (unsigned)count);
    }
}

/* ========== 内部接口（主循环调用） ========== */

/**
 * @brief 初始化事件系统（由 tf_os_init 调用）
 */
void tf_os_event_init(void)
{
    event_node_count = 0;

    for (uint8_t i = 0; i < TF_OS_EVENT_SLOT_MAX; i++) {
        os_event_slots[i].count         = 0;
        os_event_slots[i].callback_head = -1;
    }

    for (uint8_t i = 0; i < TF_OS_EVENT_NODE_MAX; i++) {
        os_event_pool[i].callback = NULL;
        os_event_pool[i].arg      = NULL;
        os_event_pool[i].next     = -1;
    }
}

/**
 * @brief 执行一轮事件处理（由 tf_os_main_loop 调用）
 */
void tf_os_event_process(void)
{
    for (uint8_t i = 0; i < TF_OS_EVENT_SLOT_MAX; i++) {
        if (os_event_slots[i].count > 0) {
            TF_DEBUG("EVENT", "process: slot=%u count=%u\n",
                     (unsigned)i, (unsigned)os_event_slots[i].count);

            /* 遍历该事件所有回调并执行 */
            for (int8_t j = os_event_slots[i].callback_head;
                 j >= 0;
                 j = os_event_pool[j].next) {
                os_event_pool[j].callback(os_event_pool[j].arg);
            }

            /* 减计数（减到 0 自动转为空闲） */
            os_event_slots[i].count--;
        }
    }
}
