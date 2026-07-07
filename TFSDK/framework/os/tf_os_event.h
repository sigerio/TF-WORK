/**
 * @file    tf_os_event.h
 * @brief   OS 事件任务管理 —— 事件驱动回调
 *
 * ============ 特性 ============
 *
 * - 初始化时注册，通过事件 ID 将回调与事件关联
 * - 同一事件可注册多个回调（订阅模式）
 * - count 状态机：>0 = 忙碌（主循环执行并自减），==0 = 空闲（允许 trigger 写入）
 * - tf_os_event_trigger：仅当 count==0 时写入，count>0 时静默丢弃（不排队、不合并）
 * - 每轮主循环对该事件的所有回调执行一次，count 减 1，减到 0 转为空闲
 * - 回调可携带参数 void *arg
 * - 不考虑并发保护——用户保证同一事件不会多处并发 trigger
 *
 * ============ 使用示例 ============
 *
 * @code
 *   #include "os/tf_os.h"
 *
 *   // 用户定义事件枚举
 *   typedef enum {
 *       E_EVENT_BUTTON_PRESS,
 *       E_EVENT_NET_OK,
 *       E_EVENT_MAX   // 须 ≤ TF_OS_EVENT_SLOT_MAX
 *   } my_event_id_e;
 *
 *   void btn_cb(void *arg) {
 *       TF_PRINTF("Button pressed!\n");
 *       (void)arg;
 *   }
 *
 *   // 注册（初始化时）
 *   tf_os_soft_event_task(0, btn_cb, NULL, 10);
 *
 *   // 触发（ISR 或业务代码）
 *   tf_os_event_trigger(0, 1);
 * @endcode
 */

#ifndef TF_OS_EVENT_H
#define TF_OS_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置宏 ========== */

/** @brief 最大事件回调节点数（总注册数），可通过 -D 覆盖 */
#ifndef TF_OS_EVENT_NODE_MAX
#define TF_OS_EVENT_NODE_MAX   16
#endif

/** @brief 最大事件 ID 数（事件槽数），可通过 -D 覆盖 */
#ifndef TF_OS_EVENT_SLOT_MAX
#define TF_OS_EVENT_SLOT_MAX   16
#endif

/* ========== 对外接口 ========== */

/**
 * @brief 注册事件回调（初始化时调用）
 *
 * 将回调函数与事件 ID 关联。同一事件可注册多个回调（订阅模式），
 * 按优先级排序执行。
 *
 * @param event_id   事件 ID（须 < TF_OS_EVENT_SLOT_MAX）
 * @param callback   回调函数指针
 * @param arg        回调参数（可为 NULL）
 * @param priority   优先级（0~63，0=最高，63=最低）
 */
void tf_os_soft_event_task(uint8_t event_id, void (*callback)(void *),
                           void *arg, uint8_t priority);

/**
 * @brief 触发事件（ISR 或业务代码中调用）
 *
 * 仅当该事件 count==0 时写入 count 值；count>0 时静默丢弃。
 * 这意味着正在处理中的事件不会累积多次触发。
 *
 * @param event_id  事件 ID（须 < TF_OS_EVENT_SLOT_MAX）
 * @param count     触发次数（通常为 1）
 */
void tf_os_event_trigger(uint8_t event_id, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* TF_OS_EVENT_H */
