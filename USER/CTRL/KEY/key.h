#ifndef KEY_H
#define KEY_H

#include <stdint.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TF_KEY_MAX_MS
#define TF_KEY_MAX_MS 4000U
#endif

#ifndef TF_KEY_LONG_MS
#define TF_KEY_LONG_MS 3000U
#endif

#ifndef TF_KEY_SHORT_MIN_MS
#define TF_KEY_SHORT_MIN_MS 100U
#endif

#ifndef TF_KEY_SHORT_MAX_MS
#define TF_KEY_SHORT_MAX_MS 2000U
#endif

typedef enum {
#define TF_KEY_ADD(n) TF_KEY_ID_##n,
#include "tf_key_list.h"
#undef TF_KEY_ADD

    TF_KEY_ID_COUNT
} tf_key_id_t;

typedef enum {
    TF_KEY_EVENT_RESET = 0,
    TF_KEY_EVENT_SHORT,
    TF_KEY_EVENT_LONG,
    TF_KEY_EVENT_LONG_REPEAT
} tf_key_event_t;

typedef enum {
    TF_KEY_STATE_IDLE = 0,
    TF_KEY_STATE_DOWN,
    TF_KEY_STATE_UP
} tf_key_state_t;

typedef enum {
    TF_KEY_MODE_NONE = 0,
    TF_KEY_MODE_ONE,
    TF_KEY_MODE_MULTI
} tf_key_mode_t;

typedef struct {
    uint16_t key_state[TF_KEY_ID_COUNT];       /* 每个按键的内部去抖状态。 */
    uint16_t key_time[TF_KEY_ID_COUNT];        /* 每个按键本次按下持续时间，单位 ms。 */
    tf_key_event_t key_event[TF_KEY_ID_COUNT]; /* 每个按键待消费的短按/长按事件。 */
} tf_key_info_t;

typedef struct {
    uint16_t key_state;       /* 组合按键的内部去抖状态。 */
    uint16_t key_time;        /* 组合按键本次按下持续时间，单位 ms。 */
    tf_key_event_t key_event; /* 组合按键待消费事件。 */
} tf_key_multi_info_t;

extern tf_key_info_t g_tf_key_info;           /* 单按键扫描的实时状态。 */
extern tf_key_multi_info_t g_tf_key_multi_info; /* 组合按键扫描的实时状态。 */

/* 初始化按键 GPIO 和软件状态。 */
bool tf_key_init(void);
/* 复位全部按键的去抖状态、计时和待消费事件。 */
void tf_key_reset_state(void);
/* 按 cycle_ms 扫描、去抖并生成按键事件。 */
void tf_key_scan_task(uint32_t cycle_ms);
/* 清除全部已生成但尚未处理的按键事件。 */
void tf_key_clear_event(void);

/* 查询指定按键是否产生指定事件，不消费该事件。 */
bool tf_key_read_event(tf_key_id_t key, tf_key_event_t event);

/* 读取状态快照、当前有效状态和即时物理按下状态。 */
tf_key_info_t tf_key_get_info(void);
bool tf_key_is_active(tf_key_id_t key);
bool tf_key_is_pressed_now(tf_key_id_t key);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
