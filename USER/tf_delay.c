/**
 * @file    tf_delay.c
 * @brief   统一时间管理 —— 延时 / 时间基准 / OS tick
 *
 * SysTick 由 main() 在启动阶段配置为 1ms 周期。
 */

#include "tf_delay.h"
#include "gd32f10x.h"
#include "gd32f10x_misc.h"

/* ========== 运行时频率 ========== */

#define TF_SYSTICK_PERIOD_US  1000U

static uint32_t sysclk_frequency;
static volatile uint32_t time_ms;

void tf_delay_systick_irq_handler(void)
{
    time_ms++;
}

void tf_time_init(void)
{
    sysclk_frequency = SystemCoreClock;
    SysTick->CTRL = 0U;
    systick_clksource_set(SYSTICK_CLKSOURCE_HCLK);
    SysTick->LOAD = sysclk_frequency / TF_SYSTICK_PERIOD_US - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

uint32_t tf_time_ms(void)
{
    return time_ms;
}

#define TF_DELAY_COEFF  240

void tf_delay_ms(uint32_t ms)
{
    if (ms == 0) return;

    uint32_t start = tf_time_ms();
    while (tf_time_ms() - start < ms) {
        /* busy wait — 不阻塞中断，不产生中断 */
    }
}

void tf_delay_us(uint32_t us)
{
    if (us == 0) return;

    if (us >= TF_SYSTICK_PERIOD_US) {
        uint32_t wait_ms = (us + TF_SYSTICK_PERIOD_US - 1U) / TF_SYSTICK_PERIOD_US;
        uint32_t start = tf_time_ms();
        while (tf_time_ms() - start < wait_ms) {
            /* busy wait */
        }
        return;
    }

    /* 回退：NOP 循环（粗精度） */
    uint32_t n = us * ((sysclk_frequency / 1000000UL) * TF_DELAY_COEFF) / 1000UL;
    if (n == 0) n = 1;
    while (n--) {
        __asm__ volatile("nop");
    }
}

uint32_t tf_os_tick_get(void)
{
    return tf_time_ms();
}
