#include "key_event.h"

#include "key.h"
#include "../MOTO/module_moto_business.h"

static void tf_key_heliped_short_event(void)
{
    /* 短按切换停机坪伸出/缩回，方向由 business 内部依当前状态判定 */
    module_moto_business_toggle_heliped();
}

static void tf_key_heliped_long_event(void)
{
    /* 长按不控制停机坪（历史遗留：仅短按控制） */
}

static void tf_key_heliped_long_repeat_event(void)
{
}

static void tf_key_door_short_event(void)
{
    /* 短按切换门开启/关闭，方向由 business 内部依当前状态判定 */
    module_moto_business_toggle_door();
}

static void tf_key_door_long_event(void)
{
    /* 长按不控制门（历史遗留：仅短按控制） */
}

static void tf_key_door_long_repeat_event(void)
{
}

void tf_key_event_callback(void)
{
    tf_key_info_t key_info = tf_key_get_info();

    switch (key_info.key_event[TF_KEY_ID_HELIPED]) {
    case TF_KEY_EVENT_SHORT:
        tf_key_heliped_short_event();
        break;
    case TF_KEY_EVENT_LONG:
        tf_key_heliped_long_event();
        break;
    case TF_KEY_EVENT_LONG_REPEAT:
        tf_key_heliped_long_repeat_event();
        break;
    default:
        break;
    }

    switch (key_info.key_event[TF_KEY_ID_DOOR]) {
    case TF_KEY_EVENT_SHORT:
        tf_key_door_short_event();
        break;
    case TF_KEY_EVENT_LONG:
        tf_key_door_long_event();
        break;
    case TF_KEY_EVENT_LONG_REPEAT:
        tf_key_door_long_repeat_event();
        break;
    default:
        break;
    }
    tf_key_clear_event();
}
