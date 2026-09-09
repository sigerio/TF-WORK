#include "CONTROL/key/key_event.h"

#include "CONTROL/key/key_input.h"
#include "CONTROL/motor/motor_control.h"

static void key_platform_short_event(void)
{
    /* 短按切换停机坪伸出/缩回，方向由 business 内部依当前状态判定 */
    motor_control_toggle_platform();
}

static void key_platform_long_event(void)
{
    /* 长按不控制停机坪（历史遗留：仅短按控制） */
}

static void key_platform_long_repeat_event(void)
{
}

static void key_door_short_event(void)
{
    /* 短按切换门开启/关闭，方向由 business 内部依当前状态判定 */
    motor_control_toggle_door();
}

static void key_door_long_event(void)
{
    /* 长按不控制门（历史遗留：仅短按控制） */
}

static void key_door_long_repeat_event(void)
{
}

void key_event_callback(void)
{
    key_info_t key_info = key_get_info();

    switch (key_info.key_event[KEY_ID_PLATFORM]) {
    case KEY_EVENT_SHORT:
        key_platform_short_event();
        break;
    case KEY_EVENT_LONG:
        key_platform_long_event();
        break;
    case KEY_EVENT_LONG_REPEAT:
        key_platform_long_repeat_event();
        break;
    default:
        break;
    }

    switch (key_info.key_event[KEY_ID_DOOR]) {
    case KEY_EVENT_SHORT:
        key_door_short_event();
        break;
    case KEY_EVENT_LONG:
        key_door_long_event();
        break;
    case KEY_EVENT_LONG_REPEAT:
        key_door_long_repeat_event();
        break;
    default:
        break;
    }
    key_clear_event();
}
