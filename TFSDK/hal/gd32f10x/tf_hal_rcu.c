/**
 * @file    tf_hal_rcu.c
 * @brief   GD32F10x 系统时钟 HAL 实现
 *
 * 系统时钟硬件配置由 CMSIS SystemInit() 完成（通过 system_gd32f10x.c 的宏选择），
 * HAL 层在此之上提供：
 *   1. 将系统频率通知 delay/OS 模块（tf_hal_system_clock_config）
 *   2. 运行时频率查询（tf_hal_rcu_clock_freq_get）
 *
 * ============ 时钟配置约定 ============
 *
 * 时钟初始化不在 HAL 层重复实现，而是通过 CMSIS 宏管理。
 * 当前工程使用内部 IRC8M 振荡器，不使用外部晶振 HXTAL。
 *
 *   CMSIS/system_gd32f10x.c 中启用：
 *     #define __SYSTEM_CLOCK_108M_PLL_IRC8M
 *
 *   对应时钟树（IRC8M 内部振荡器，无外部晶振 HXTAL）：
 *
 *     IRC8M (8MHz) ─→ /2 ─→ 4MHz ─→ ×27 ─→ PLL (108MHz)
 *                                                │
 *                                                ├→ SYSCLK = 108MHz
 *                                                │     ├→ AHB   = 108MHz (/1)
 *                                                │     ├→ APB2  = 108MHz (/1)
 *                                                │     └→ APB1  = 54MHz  (/2)
 *
 * @note system_gd32f10x.c 中的 __SYSTEM_CLOCK_xxx 是 .c 文件内部宏，
 *       其他文件不可见。tf_sysclk_freq_set() 直接使用硬编码值。
 * @see   CMSIS/system_gd32f10x.c  system_clock_108m_irc8m()
 *
 * @note 验证宏：__SYSTEM_CLOCK_108M_PLL_IRC8M 定义在 system_gd32f10x.c 第 51 行，
 *       值为 (uint32_t)(108000000)。该宏为 .c 内部宏，其他文件不可见，
 *       因此 tf_sysclk_freq_set() 中直接使用硬编码值 108000000UL。
 */

#include "gd32f10x.h"
#include "gd32f10x_rcu.h"
#include "../tf_hal_rcu.h"
#include "../../drv/tf_drv_rcu.h"
#include "../../config/tf_board_mcu.h"
#include "../../framework/tf_delay.h"

#if MCU_CONTAIN_PRE_RCU

/**
 * @brief 系统时钟初始化入口
 *
 * 硬件时钟已在 CMSIS SystemInit() 中根据 system_gd32f10x.c 的宏定义完成配置。
 * 本函数只负责将系统频率通知框架层（delay/OS 模块），不再重复配置硬件。
 *
 * @note system_gd32f10x.c 中应使用 __SYSTEM_CLOCK_108M_PLL_IRC8M，
 *       而非默认的 __SYSTEM_CLOCK_108M_PLL_HXTAL（无外部晶振会卡死）。
 */
void tf_hal_system_clock_config(void)
{
    /* 更新 delay/OS 模块的运行时频率，确保延时精度 */
    tf_sysclk_freq_set(108000000UL);
}

/**
 * @brief 获取系统时钟频率
 *
 * 直接委托 GD32 固件库 `rcu_clock_freq_get()`，读取寄存器实时计算。
 * 返回值反映当前运行中的实际频率，而非预设值。
 *
 * @param clock_id  TF 层时钟 ID（见 tf_drv_rcu_clock_e 枚举）
 * @return          频率（Hz），无效 clock_id 返回 0
 *
 * @note clock_id 到 GD32 枚举的映射关系：
 *         E_DRV_RCU_CLOCK_SYSCLK (10) → CK_SYS  (0)
 *         E_DRV_RCU_CLOCK_AHB    (11) → CK_AHB  (1)
 *         E_DRV_RCU_CLOCK_APB1   (12) → CK_APB1 (2)
 *         E_DRV_RCU_CLOCK_APB2   (13) → CK_APB2 (3)
 */
uint32_t tf_hal_rcu_clock_freq_get(uint32_t clock_id)
{
    uint32_t freq = 0UL;

    switch ((tf_drv_rcu_clock_e)clock_id) {
    case E_DRV_RCU_CLOCK_SYSCLK:
        freq = rcu_clock_freq_get(CK_SYS);
        break;

    case E_DRV_RCU_CLOCK_AHB:
        freq = rcu_clock_freq_get(CK_AHB);
        break;

    case E_DRV_RCU_CLOCK_APB1:
        freq = rcu_clock_freq_get(CK_APB1);
        break;

    case E_DRV_RCU_CLOCK_APB2:
        freq = rcu_clock_freq_get(CK_APB2);
        break;

    default:
        /* E_DRV_RCU_CLOCK_APB3 (14) — GD32F10x 无 APB3，返回 0 */
        break;
    }

    return freq;
}

#endif /* MCU_CONTAIN_PRE_RCU */
