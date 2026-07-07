/**
 * @file    tf_drv_rcu.c
 * @brief   RCU 系统时钟配置与管理
 *
 * 本模块为系统时钟的单一入口。各平台 HAL 层自包含完整时钟配置：
 *   - tf_hal_system_clock_config()：晶振/PLL/总线分频全在此函数内完成
 *   - tf_hal_rcu_clock_freq_get()：运行时频率查询
 *
 * ============ 架构说明 ============
 *
 * 时钟参数不通过配置表传入，而是由各平台 HAL 层直接写死代码中。
 * 外设级时钟门控/复位属于各外设 HAL 初始化内部职责，不通过公共 RCU DRV 暴露。
 * 这种设计避免了 DRV 层对不同 MCU PLL 架构（GD32 MUL vs STM32H5 VCO）
 * 的复杂适配，各平台代码各自保持简洁。
 */

#include "tf_drv_rcu.h"
#include "../hal/tf_hal_rcu.h"
#include "../framework/tf_debug.h"
 

#define TF_DRV_RCU_DEVICE_COUNT 1U

/* ========== 框架内部状态 ========== */

static bool rcu_registered[TF_DRV_RCU_DEVICE_COUNT];

/* ========== 对外函数实现 ========== */

tf_drv_rcu_handle_t tf_drv_rcu_register(int id)
{

    if (id < 0 || id >= (int)TF_DRV_RCU_DEVICE_COUNT) {
        return NULL;
    }

    if (rcu_registered[id]) {
        return (tf_drv_rcu_handle_t)&rcu_registered[id];
    }

    /* 调用 HAL 层系统时钟配置（各平台自包含完整时钟树设置） */
    tf_hal_system_clock_config();

    rcu_registered[id] = true;

    return (tf_drv_rcu_handle_t)&rcu_registered[id];
}

tf_drv_rcu_handle_t tf_drv_rcu_get_handle(int id)
{

    if (id >= (int)TF_DRV_RCU_DEVICE_COUNT) {
        return NULL;
    }

    if (!rcu_registered[id]) {
        return NULL;
    }

    return (tf_drv_rcu_handle_t)&rcu_registered[id];
}

bool tf_drv_rcu_is_registered(int id)
{
    bool ret = (id >= 0 && id < (int)TF_DRV_RCU_DEVICE_COUNT) ? rcu_registered[id] : false;
    return ret;
}

uint32_t tf_drv_rcu_clock_freq_get(tf_drv_rcu_clock_e clock_id)
{
    uint32_t freq = tf_hal_rcu_clock_freq_get((uint32_t)clock_id);
    return freq;
}
