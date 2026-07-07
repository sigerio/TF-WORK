/**
 * @file    tf_drv_fwdgt.c
 * @brief   FWDGT 配置与控制实现
 *
 * 超时参数由 HAL 层硬编码（~1.5-1.9s），不通过 config 传入。
 * register() 直接委托 HAL 层初始化和启动看门狗。
 */

#include "tf_drv_fwdgt.h"
#include "../hal/tf_hal_fwdgt.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_IWDG

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif

/* ========== 运行时状态 ========== */

static bool fwdgt_registered[E_DRV_FWDGT_MAX];

/* ========== 对外函数实现 ========== */

tf_drv_fwdgt_handle_t tf_drv_fwdgt_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_FWDGT_MAX);
    if (id < 0 || id >= E_DRV_FWDGT_MAX) {
        return NULL;
    }

    if (fwdgt_registered[id]) {
        return (tf_drv_fwdgt_handle_t)&fwdgt_configs[id];
    }

    /* 初始化并启动看门狗（参数由 HAL 层硬编码：~1.5-1.9s） */
    tf_hal_fwdgt_init();

    fwdgt_registered[id] = true;

    return (tf_drv_fwdgt_handle_t)&fwdgt_configs[id];
}

tf_drv_fwdgt_handle_t tf_drv_fwdgt_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_FWDGT_MAX) {
        return NULL;
    }

    if (!fwdgt_registered[id]) {
        return NULL;
    }

    return (tf_drv_fwdgt_handle_t)&fwdgt_configs[id];
}

bool tf_drv_fwdgt_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_FWDGT_MAX)
                ? fwdgt_registered[id] : false;
    return ret;
}

void tf_drv_fwdgt_feed(tf_drv_fwdgt_handle_t h)
{
    TF_ASSERT(h != NULL); if (h == NULL) return;
    (void)h;
    tf_hal_fwdgt_counter_reload();
}
#endif /* MCU_CONTAIN_PRE_IWDG */
