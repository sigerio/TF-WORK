/**
 * @file    os_event.h
 * @brief   OS 事件任务管理 —— 事件驱动回调
 *
 * ============ 特性 ============
 *
 * - 初始化时注册，通过事件 ID 将回调与事件关联
 * - 每个事件 ID 只绑定一个回调
 * - 事件 pool 按注册顺序保存，调度 slot 按优先级/注册顺序排序
 * - count 状态机：>0 = 忙碌（主循环执行并自减），==0 = 空闲（允许 trigger 写入）
 * - os_event_trigger：同一事件重复触发时，新参数覆盖旧参数
 * - 每轮主循环按调度 slot 顺序执行 pending 事件，count 减 1，减到 0 转为空闲
 * - trigger 时可携带参数，event 内部从专用内存池分配并拷贝
 * - 不考虑并发保护——用户保证同一事件不会多处并发 trigger
 *
 * ============ 使用示例 ============
 *
 * @code
 *   #include "OS/os.h"
 *
 *   // 用户定义事件枚举
 *   typedef enum {
 *       E_EVENT_BUTTON_PRESS,
 *       E_EVENT_NET_OK,
 *       E_EVENT_MAX   // 须 ≤ OS_EVENT_SLOT_MAX
 *   } my_event_id_e;
 *
 *   void btn_cb(void *arg) {
 *       PRINTF("Button pressed!\n");
 *       (void)arg;
 *   }
 *
 *   // 注册（初始化时）
 *   os_soft_event_task(0, btn_cb, 10);
 *
 *   // 触发（业务代码）
 *   os_event_trigger(0, NULL, 0U, 1);
 * @endcode
 */

#ifndef OS_EVENT_H
#define OS_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */

/** @brief 最大事件回调节点数（总注册数），可通过 -D 覆盖 */
#ifndef OS_EVENT_NODE_MAX
#define OS_EVENT_NODE_MAX   16
#endif

/** @brief 最大事件 ID 数（事件槽数），可通过 -D 覆盖 */
#ifndef OS_EVENT_SLOT_MAX
#define OS_EVENT_SLOT_MAX   16
#endif

/** @brief event 参数内存池大小（字节），可通过 -D 覆盖 */
#ifndef OS_EVENT_HEAP_SIZE
#define OS_EVENT_HEAP_SIZE   512U
#endif

/* ========== 对外接口 ========== */

/**
 * @brief 注册事件回调（初始化时调用）
 *
 * 将回调函数与事件 ID 关联。每个事件 ID 只绑定一个回调。
 * 事件之间按优先级排序执行；同优先级按注册顺序执行。
 *
 * @param event_id   事件 ID（须 < OS_EVENT_SLOT_MAX）
 * @param callback   回调函数指针
 * @param priority   优先级（0~63，0=最高，63=最低）
 */
void os_soft_event_task(uint8_t event_id, void (*callback)(void *), uint8_t priority);

/**
 * @brief 触发事件（业务代码中调用）
 *
 * 仅当该事件 count==0 时写入 count 值；count>0 时只覆盖参数，
 * 不累积触发次数。
 * 当 arg 非 NULL 且 arg_size > 0 时，event 内部会从专用内存池
 * 分配 arg_size 字节并拷贝参数，回调收到的是 event 内部副本。
 *
 * @param event_id  事件 ID（须 < OS_EVENT_SLOT_MAX）
 * @param arg       触发参数源地址（可为 NULL）
 * @param arg_size  触发参数字节数
 * @param count     触发次数（通常为 1）
 *
 * @return 0=成功，负数=失败
 */
int8_t os_event_trigger(uint8_t event_id, const void *arg, uint16_t arg_size, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* OS_EVENT_H */
