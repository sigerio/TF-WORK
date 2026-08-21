/**
 * @file    tf_os_timer.c
 * @brief   OS 定时任务管理实现
 *
 * 使用静态数组 + 优先级排序链表管理周期定时任务。
 * 时间基准来源：tf_delay.c 统一时间基准，由 HAL 层提供底层实现。
 * 主循环遍历链表检查到期，到期执行并更新 last_tick。
 */

#include "tf_os_timer.h"
 
#include "../../tf_debug.h"

/* ========== 内部数据结构 ========== */

/**
 * @brief 定时任务节点
 *
 * 主循环读 last_tick 判断到期，无需 state 标记。
 */
typedef struct {
    void     (*callback)(void *);   /**< 回调函数指针           */
    void      *arg;                 /**< 回调参数               */
    uint8_t    priority;            /**< 优先级（0~63）         */
    int8_t     next;                /**< 链表下一节点索引，-1=尾 */
    uint32_t   period_ms;           /**< 周期（毫秒）           */
    uint32_t   last_tick;           /**< 上次执行时的 tick 值   */
} tf_os_timer_node_t;

/* ========== 静态数据 ========== */

/** @brief 定时任务节点池 */
static tf_os_timer_node_t os_timer_pool[TF_OS_TIMER_MAX];

/** @brief 链表头索引（-1 = 空链表） */
static int8_t timer_head = -1;

/** @brief 已注册节点计数 */
static uint8_t timer_node_count = 0;

/* ========== 对外接口实现 ========== */

void tf_os_soft_timer_task(void (*callback)(void *), void *arg,
                           uint32_t period_ms, uint8_t priority)
{
    if (timer_node_count >= TF_OS_TIMER_MAX) {
        TF_DEBUG("TIMER", "soft_timer_task: pool full (%u/%u)\n",
                 (unsigned)timer_node_count, (unsigned)TF_OS_TIMER_MAX);
        return;
    }

    /* 分配新节点 */
    int8_t node_idx = (int8_t)timer_node_count;
    tf_os_timer_node_t *node = &os_timer_pool[node_idx];

    node->callback  = callback;
    node->arg       = arg;
    node->priority  = priority;
    node->period_ms = period_ms;
    node->last_tick = 0;
    node->next      = -1;

    /* 按优先级插入链表（高优先级靠前，同优先级按注册先后） */
    int8_t *prev = &timer_head;
    int8_t  curr = timer_head;

    while (curr >= 0) {
        if (os_timer_pool[curr].priority > priority) {
            break;  /* 当前节点优先级更低，插入在此前 */
        }
        prev = &os_timer_pool[curr].next;
        curr = os_timer_pool[curr].next;
    }

    node->next = curr;
    *prev = node_idx;
    timer_node_count++;

    TF_DEBUG("TIMER", "soft_timer_task: node=%d cb=%p period=%u pri=%u "
             "total=%u\n",
             node_idx, (void *)callback, (unsigned)period_ms,
             (unsigned)priority, (unsigned)timer_node_count);
}

/* ========== 内部接口（主循环调用） ========== */

/**
 * @brief 初始化定时任务池（由 tf_os_init 调用）
 */
void tf_os_timer_init(void)
{
    timer_head = -1;
    timer_node_count = 0;

    for (uint8_t i = 0; i < TF_OS_TIMER_MAX; i++) {
        os_timer_pool[i].callback  = NULL;
        os_timer_pool[i].arg       = NULL;
        os_timer_pool[i].next      = -1;
    }
}

/**
 * @brief 执行一轮定时任务扫描（由 tf_os_main_loop 调用）
 *
 * @param now  当前 tick 值（tf_os_tick_get）
 */
void tf_os_timer_process(uint32_t now)
{
    for (int8_t i = timer_head; i >= 0; i = os_timer_pool[i].next) {
        tf_os_timer_node_t *node = &os_timer_pool[i];

        if (now - node->last_tick >= node->period_ms) {
            // TF_DEBUG("TIMER", "process: node=%d cb=%p period=%u "
            //          "diff=%u\n",
            //          i, (void *)node->callback,
            //          (unsigned)node->period_ms,
            //          (unsigned)(now - node->last_tick));
            node->last_tick = now;
            node->callback(node->arg);
        }
    }
}
