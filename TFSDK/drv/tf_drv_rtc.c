/**
 * @file    tf_drv_rtc.c
 * @brief   RTC 实时时钟配置与控制实现
 *
 * RTC 初始化序列（参考 GD32F10x Calendar_demo）：
 *
 *   register(id)
 *   1. 使能 LXTAL → 等待稳定
 *   2. 选择 RTC 时钟源（LXTAL）
 *   3. 进入配置模式
 *   4. 设预分频
 *   5. 设初始计数器值
 *   6. 设闹钟值
 *   7. 退出配置模式
 *   8. 等待寄存器同步
 */

#include "tf_drv_rtc.h"
#include "../hal/tf_hal_rtc.h"
#include "../framework/tf_debug.h"
#include "../framework/tf_delay.h"
#if MCU_CONTAIN_PRE_RTC

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif


/* ========== 运行时状态 ========== */

static bool rtc_registered[E_DRV_RTC_MAX];

/* ========== 对外函数实现 ========== */

tf_drv_rtc_handle_t tf_drv_rtc_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_RTC_MAX);
    if (id < 0 || id >= E_DRV_RTC_MAX) {
        return NULL;
    }

    if (rtc_registered[id]) {
        return (tf_drv_rtc_handle_t)&rtc_configs[id];
    }


    tf_hal_rtc_init();

    rtc_registered[id] = true;

    return (tf_drv_rtc_handle_t)&rtc_configs[id];
}

tf_drv_rtc_handle_t tf_drv_rtc_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_RTC_MAX) {
        return NULL;
    }

    if (!rtc_registered[id]) {
        return NULL;
    }

    return (tf_drv_rtc_handle_t)&rtc_configs[id];
}

bool tf_drv_rtc_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_RTC_MAX) ? rtc_registered[id] : false;
    return ret;
}

/* ========== 内部辅助 ========== */

/** 从句柄获取 id，验证注册状态，返回 true=有效 */
static bool rtc_validate(tf_drv_rtc_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return false;
    int id = (tf_drv_rtc_config_t *)h - rtc_configs;
    return (id >= 0 && id < E_DRV_RTC_MAX && rtc_registered[id]);
}

/* ---- 计数器操作 ---- */

void tf_drv_rtc_counter_set(tf_drv_rtc_handle_t h, uint32_t cnt)
{
    if (!rtc_validate(h)) return;
    tf_hal_rtc_lwoff_wait();
    tf_hal_rtc_counter_set(cnt);
}

uint32_t tf_drv_rtc_counter_get(tf_drv_rtc_handle_t h)
{
    if (!rtc_validate(h)) return 0;
    return tf_hal_rtc_counter_get();
}

/* ---- 预分频 ---- */

void tf_drv_rtc_prescaler_set(tf_drv_rtc_handle_t h, uint32_t psc)
{
    if (!rtc_validate(h)) return;
    tf_hal_rtc_lwoff_wait();
    tf_hal_rtc_config_mode_enter();
    tf_hal_rtc_lwoff_wait();
    tf_hal_rtc_prescaler_set(psc);
    tf_hal_rtc_config_mode_exit();
    tf_hal_rtc_register_sync_wait();
}

/* ---- 闹钟 ---- */

void tf_drv_rtc_alarm_config(tf_drv_rtc_handle_t h, uint32_t alarm)
{
    if (!rtc_validate(h)) return;
    tf_hal_rtc_lwoff_wait();
    tf_hal_rtc_alarm_config(alarm);
}
#endif /* MCU_CONTAIN_PRE_RTC */
