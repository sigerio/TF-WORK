#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include <stdint.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KEY_MAX_MS
#define KEY_MAX_MS 4000U
#endif

#ifndef KEY_LONG_MS
#define KEY_LONG_MS 3000U
#endif

#ifndef KEY_SHORT_MIN_MS
#define KEY_SHORT_MIN_MS 100U
#endif

#ifndef KEY_SHORT_MAX_MS
#define KEY_SHORT_MAX_MS 2000U
#endif

typedef enum {
#define KEY_ADD(n) KEY_ID_##n,
#include "CONTROL/key/key_list.h"
#undef KEY_ADD

    KEY_ID_COUNT
} key_id_t;

typedef enum {
    KEY_EVENT_RESET = 0,
    KEY_EVENT_SHORT,
    KEY_EVENT_LONG,
    KEY_EVENT_LONG_REPEAT
} key_event_t;

typedef enum {
    KEY_STATE_IDLE = 0,
    KEY_STATE_DOWN,
    KEY_STATE_UP
} key_state_t;

typedef enum {
    KEY_MODE_NONE = 0,
    KEY_MODE_ONE,
    KEY_MODE_MULTI
} key_mode_t;

typedef struct {
    uint16_t key_state[KEY_ID_COUNT];       /* 每个按键的内部去抖状态。 */
    uint16_t key_time[KEY_ID_COUNT];        /* 每个按键本次按下持续时间，单位 ms。 */
    key_event_t key_event[KEY_ID_COUNT]; /* 每个按键待消费的短按/长按事件。 */
} key_info_t;

typedef struct {
    uint16_t key_state;       /* 组合按键的内部去抖状态。 */
    uint16_t key_time;        /* 组合按键本次按下持续时间，单位 ms。 */
    key_event_t key_event; /* 组合按键待消费事件。 */
} key_multi_info_t;

extern key_info_t g_tf_key_info;           /* 单按键扫描的实时状态。 */
extern key_multi_info_t g_tf_key_multi_info; /* 组合按键扫描的实时状态。 */

/* 初始化按键 GPIO 和软件状态。 */
bool key_init(void);
/* 复位全部按键的去抖状态、计时和待消费事件。 */
void key_reset_state(void);
/* 按 cycle_ms 扫描、去抖并生成按键事件。 */
void key_scan_task(uint32_t cycle_ms);
/* 清除全部已生成但尚未处理的按键事件。 */
void key_clear_event(void);

/* 查询指定按键是否产生指定事件，不消费该事件。 */
bool key_read_event(key_id_t key, key_event_t event);

/* 读取状态快照、当前有效状态和即时物理按下状态。 */
key_info_t key_get_info(void);
bool key_is_active(key_id_t key);
bool key_is_pressed_now(key_id_t key);

#ifdef __cplusplus
}
#endif

#endif /* KEY_INPUT_H */
