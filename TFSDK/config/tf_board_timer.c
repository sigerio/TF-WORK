/**
 * @file    tf_board_timer.c
 * @brief   板级定时器资源配置
 *
 * 当前工程仅使用 GD32 TIMER2_CH1 (PA7) 用于 PWM 输出。
 *
 * 参数说明（参考 GD32F10x TIMER1_pwmout 官方例程）：
 *   SystemCoreClock = 108MHz (IRC8M PLL)
 *   prescaler = 107  →  TIMER_CLK = 108MHz / 108 = 1MHz
 *   period    = 19999 →  PWM 频率 = 1MHz / 20000 = 50Hz
 *
 * MCU: GD32F103RE/RC (High Density)
 * 固件库: GD32F10x Firmware Library V2.7.0
 */

#include "tf_board_timer.h"
#include "gd32f10x_timer.h"   /* TIMER2, TIMER_CH_1 */

#if MCU_CONTAIN_PRE_TIM

tf_drv_timer_config_t timer_configs[E_DRV_TIMER_MAX] = {
    [E_DRV_TIMER_2] = {
        .tim           = (void *)TIMER2,
        .prescaler     = 107,         /* 108MHz / 108 = 1MHz */
        .period        = 19999,       /* 1MHz / 20000 = 50Hz */
        .ch = {
            [E_DRV_T2_CH1] = { .io_id = E_IO_T2_CH1, .channel = TIMER_CH_1, .enabled = 1 },
        },
    },
};

#endif /* MCU_CONTAIN_PRE_TIM */
