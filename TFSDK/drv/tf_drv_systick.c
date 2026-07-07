/**
 * @file    tf_drv_systick.c
 * @brief   SysTick 系统滴答定时器驱动实现
 */

#include "tf_drv_systick.h"
#include "../framework/tf_delay.h"
#include <stddef.h>

static bool systick_registered = false;

tf_drv_systick_handle_t tf_drv_systick_register(void)
{
    if (!systick_registered) {
        tf_time_init();
        systick_registered = true;
    }

    return (tf_drv_systick_handle_t)&systick_registered;
}

tf_drv_systick_handle_t tf_drv_systick_get_handle(void)
{
    if (!systick_registered) {
        return NULL;
    }

    return (tf_drv_systick_handle_t)&systick_registered;
}

bool tf_drv_systick_is_registered(void)
{
    return systick_registered;
}
