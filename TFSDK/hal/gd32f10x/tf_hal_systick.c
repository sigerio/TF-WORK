/**
 * @file    tf_hal_systick.c
 * @brief   GD32F10x SysTick 固定配置实现
 *
 * 本文件只提供 SysTick 寄存器级操作：
 * - 配置 SysTick 为 1ms 中断
 *
 * 时间累计、tf_time_xxx()、tf_delay_xxx()、tf_os_tick_get() 均在
 * tf_delay.c 中实现。
 */

#include "gd32f10x.h"
#include "gd32f10x_rcu.h"

void tf_hal_systick_config(uint32_t sysclk_hz)
{
    uint32_t ticks_per_ms;

    if (sysclk_hz == 0U) {
        sysclk_hz = rcu_clock_freq_get(CK_SYS);
    }

    if (sysclk_hz == 0U) {
        sysclk_hz = 8000000UL;
    }

    ticks_per_ms = sysclk_hz / 1000UL;

    SysTick->CTRL = 0U;
    systick_clksource_set(SYSTICK_CLKSOURCE_HCLK);
    SysTick->LOAD = ticks_per_ms - 1U;
    SysTick->VAL = 0U;

    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}
