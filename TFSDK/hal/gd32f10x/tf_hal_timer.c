/**
 * @file    tf_hal_timer.c
 * @brief   GD32F10x TIMER HAL 薄封装
 *
 * 基于 GD32F10x Firmware Library 的 TIMER 驱动实现。
 * 初始化参数（prescaler/period）由 DRV config 透传进入 HAL，
 * 不再在 HAL 层维护静态参数表。
 *
 * ============ 引脚复用说明 ============
 *
 * GD32F10x 定时器通道与 GPIO 的复用映射是硬件固定的（无 AF 号选择），
 * PA7 默认复用为 TIMER2_CH1 输出，需将 GPIO 配置为 GPIO_MODE_AF_PP。
 * GPIO 初始化由 IO 子系统（tf_drv_io）完成，本层只负责 TIMER 本体。
 *
 * ============ 参考例程 ============
 *
 * GD32F10x_Firmware_Library_V2.7.0/Examples/TIMER/TIMER1_pwmout/
 *
 * @note MCU: GD32F103RE/RC (High Density), 系统时钟 108MHz (IRC8M PLL)
 * @see GD32F10x 固件库 V2.7.0
 */

#include "gd32f10x.h"
#include "gd32f10x_timer.h"
#include "gd32f10x_rcu.h"
#include "../tf_hal_timer.h"
#include "../../drv/tf_drv_timer.h"
#include "../../framework/tf_debug.h"

/* ========== 内部辅助函数 ========== */

/** @brief 使能定时器外设时钟 */
static void gd32_timer_clock_enable(uint32_t timer_periph)
{
    if (timer_periph == TIMER0)      { rcu_periph_clock_enable(RCU_TIMER0);      return; }
    if (timer_periph == TIMER1)      { rcu_periph_clock_enable(RCU_TIMER1);      return; }
    if (timer_periph == TIMER2)      { rcu_periph_clock_enable(RCU_TIMER2);      return; }
    if (timer_periph == TIMER3)      { rcu_periph_clock_enable(RCU_TIMER3);      return; }
    if (timer_periph == TIMER4)      { rcu_periph_clock_enable(RCU_TIMER4);      return; }
    if (timer_periph == TIMER5)      { rcu_periph_clock_enable(RCU_TIMER5);      return; }
    if (timer_periph == TIMER6)      { rcu_periph_clock_enable(RCU_TIMER6);      return; }
    if (timer_periph == TIMER7)      { rcu_periph_clock_enable(RCU_TIMER7);      return; }
    TF_DEBUG("HAL", "unknown TIMER periph: 0x%08lX\n", (unsigned long)timer_periph);
}

/** @brief 禁能定时器外设时钟 */
static void gd32_timer_clock_disable(uint32_t timer_periph)
{
    if (timer_periph == TIMER0)      { rcu_periph_clock_disable(RCU_TIMER0);      return; }
    if (timer_periph == TIMER1)      { rcu_periph_clock_disable(RCU_TIMER1);      return; }
    if (timer_periph == TIMER2)      { rcu_periph_clock_disable(RCU_TIMER2);      return; }
    if (timer_periph == TIMER3)      { rcu_periph_clock_disable(RCU_TIMER3);      return; }
    if (timer_periph == TIMER4)      { rcu_periph_clock_disable(RCU_TIMER4);      return; }
    if (timer_periph == TIMER5)      { rcu_periph_clock_disable(RCU_TIMER5);      return; }
    if (timer_periph == TIMER6)      { rcu_periph_clock_disable(RCU_TIMER6);      return; }
    if (timer_periph == TIMER7)      { rcu_periph_clock_disable(RCU_TIMER7);      return; }
}

/* ========== HAL 接口实现 ========== */

void tf_hal_timer_init(const tf_drv_timer_config_t *cfg)
{
    uint32_t periph   = (uint32_t)cfg->tim;
    uint16_t prescaler = cfg->prescaler;
    uint32_t period    = cfg->period;

    /* 1. 使能时钟 */
    gd32_timer_clock_enable(periph);

    /* 2. 复位定时器 */
    timer_deinit(periph);

    /* 3. 初始化时基（prescaler / period / 计数模式） */
    timer_parameter_struct initpara;
    timer_struct_para_init(&initpara);

    initpara.prescaler         = prescaler;
    initpara.alignedmode       = TIMER_COUNTER_EDGE;
    initpara.counterdirection  = TIMER_COUNTER_UP;
    initpara.period            = period;
    initpara.clockdivision     = TIMER_CKDIV_DIV1;
    initpara.repetitioncounter = 0;

    timer_init(periph, &initpara);

    /* 4. 自动重载预装载使能 */
    timer_auto_reload_shadow_enable(periph);

    TF_DEBUG("HAL", "timer_init(0x%08lX): PSC=%u, ARR=%lu\n",
             (unsigned long)periph, (unsigned)prescaler, (unsigned long)period);
}

void tf_hal_timer_deinit(void *tim)
{
    uint32_t periph = (uint32_t)tim;
    timer_deinit(periph);
    gd32_timer_clock_disable(periph);
}

void tf_hal_timer_start(void *tim)
{
    timer_enable((uint32_t)tim);
}

void tf_hal_timer_stop(void *tim)
{
    timer_disable((uint32_t)tim);
}

uint32_t tf_hal_timer_get_cnt(void *tim)
{
    return timer_counter_read((uint32_t)tim);
}

/* ========== 通道输出使能 / 禁能 ========== */

void tf_hal_timer_ch_output_enable(void *tim, uint32_t channel)
{
    timer_channel_output_state_config((uint32_t)tim, (uint16_t)channel, TIMER_CCX_ENABLE);
}

void tf_hal_timer_ch_output_disable(void *tim, uint32_t channel)
{
    timer_channel_output_state_config((uint32_t)tim, (uint16_t)channel, TIMER_CCX_DISABLE);
}

/* ========== PWM 模式配置 ========== */

void tf_hal_timer_ch_pwm_mode_config(void *tim, uint32_t channel)
{
    uint32_t  periph = (uint32_t)tim;
    uint16_t  ch     = (uint16_t)channel;

    /* ── OC 参数：PWM mode 1, 高极性, 使能输出 ── */
    timer_oc_parameter_struct ocpara;
    timer_channel_output_struct_para_init(&ocpara);

    ocpara.outputstate  = TIMER_CCX_ENABLE;
    ocpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    ocpara.outputnstate = TIMER_CCXN_DISABLE;
    ocpara.ocnpolarity  = TIMER_OC_POLARITY_HIGH;
    ocpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    ocpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_channel_output_config(periph, ch, &ocpara);

    /* ── PWM mode 1 ── */
    timer_channel_output_mode_config(periph, ch, TIMER_OC_MODE_PWM0);

    /* ── 关闭输出比较预装载（立即生效） ── */
    timer_channel_output_shadow_config(periph, ch, TIMER_OC_SHADOW_DISABLE);

    /* ── 初始占空比为 0 ── */
    timer_channel_output_pulse_value_config(periph, ch, 0);

    TF_DEBUG("HAL", "ch_pwm_mode(0x%08lX, ch=%u)\n",
             (unsigned long)periph, (unsigned)ch);
}

/* ========== 占空比设置 ========== */

void tf_hal_timer_ch_pwm_set_duty(void *tim, uint32_t channel, uint8_t duty)
{
    uint32_t periph = (uint32_t)tim;
    uint16_t ch     = (uint16_t)channel;

    if (duty > 100U) duty = 100U;

    /* 直接从硬件寄存器读取当前 ARR 值计算占空比 */
    uint32_t period = TIMER_CAR(periph);
    if (period == 0U) return;

    uint32_t ccr = ((uint32_t)duty * period) / 100U;
    timer_channel_output_pulse_value_config(periph, ch, (uint16_t)ccr);
}

void tf_hal_timer_ch_set_ccr(void *tim, uint32_t channel, uint32_t ccr)
{
    uint32_t periph = (uint32_t)tim;
    uint16_t ch     = (uint16_t)channel;
    timer_channel_output_pulse_value_config(periph, ch, (uint16_t)ccr);
}

/* ========== 中断控制 ========== */

void tf_hal_timer_interrupt_enable(void *tim, uint32_t int_mask)
{
    timer_interrupt_enable((uint32_t)tim, int_mask);
}

void tf_hal_timer_interrupt_disable(void *tim, uint32_t int_mask)
{
    timer_interrupt_disable((uint32_t)tim, int_mask);
}

/* ========== 标志查询/清除 ========== */

uint32_t tf_hal_timer_flag_get(void *tim, uint32_t flag)
{
    return (uint32_t)timer_flag_get((uint32_t)tim, flag);
}

void tf_hal_timer_flag_clear(void *tim, uint32_t flag)
{
    timer_flag_clear((uint32_t)tim, flag);
}
