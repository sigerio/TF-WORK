/**
 * @file    tf_delay.c
 * @brief   统一时间管理 —— 延时 / 时间基准 / OS tick
 *
 * 本模块是系统时间管理的唯一入口。tf_delay_ms/us() 和 tf_os_tick_get()
 * 共享同一时间基准，HAL 层只提供固定的 SysTick 配置函数，
 * 不再通过 weak 符号覆盖 delay 的公共接口。
 *
 * ============ 设计 ============
 *
 *                 tf_delay.c（核心）
 *                 ├─ tf_time_init()      [公共入口]
 *                 ├─ tf_time_ms()        [公共入口，1ms tick]
 *                 ├─ tf_delay_ms/us()    [公共入口，阻塞延时]
 *                 ├─ tf_os_tick_get()    [公共入口，OS tick]
 *                 │
 * 对外暴露:       └────────────────────────────────────┐
 *   ┌── tf_delay_ms/us()  ──────── 轮询 ms tick ──────┤
 *   ├── tf_os_tick_get()  ──────── 返回低 32 位 ms ---┤
 *   └── tf_time_ms()      ──────── 返回 ms tick ------┘
 *
 * HAL 边界：
 *   - tf_hal_systick_config()        配置 1ms SysTick
 */

#include "tf_delay.h"

/* ========== SysTick HAL 底层函数 ==========
 *
 * 这些符号由平台 HAL 提供，只做底层配置，不属于用户 API。
 */
extern void tf_hal_systick_config(uint32_t sysclk_hz);

/* ========== 运行时频率 ========== */

static uint32_t g_sysclk_freq = 0;

void tf_sysclk_freq_set(uint32_t hz)
{
    g_sysclk_freq = hz;
}

uint32_t tf_sysclk_freq_get(void)
{
    return g_sysclk_freq ? g_sysclk_freq : (uint32_t)TF_SYSCLK_FREQ;
}

/* ==================================================================
 *  SysTick 时间基准状态
 *
 *  delay 模块维护 32 位累计毫秒 tick；HAL 只负责底层 SysTick 配置。
 *  SysTick 中断每 1ms 调用 tf_delay_systick_irq_handler()。
 * ================================================================*/

#define TF_SYSTICK_PERIOD_US  1000U

static uint8_t g_time_initialized = 0;
static volatile uint32_t g_time_ms = 0U;

static void ensure_time_init(void)
{
    if (!g_time_initialized) {
        g_time_ms = 0U;
        tf_hal_systick_config(tf_sysclk_freq_get());
        g_time_initialized = 1;
    }
}

void tf_delay_systick_irq_handler(void)
{
    g_time_ms++;
}

/* ==================================================================
 *  对外接口
 * ================================================================*/

void tf_time_init(void)
{
    ensure_time_init();
}

uint32_t tf_time_ms(void)
{
    ensure_time_init();
    return g_time_ms;
}

/* ==================================================================
 *  阻塞延时
 *
 *  ms 级延时通过 SysTick 计数轮询实现；us < 1ms 时使用 NOP 粗延时。
 * ================================================================*/

/*
 * 原理：Cortex-M3 下 volatile 空循环 ≈ 3 cycle/次
 *       (load r, cmp, bne) + 函数调用/返回开销。
 *       COEFF = 240 基于 72MHz 实测（1ms ≈ 240 × 72 ≈ 17280 循环）。
 *       偏差主要来自编译器优化和函数调用帧，可在目标板上微调。
 */
#define TF_DELAY_COEFF  240

void tf_delay_ms(uint32_t ms)
{
    if (ms == 0) return;

    ensure_time_init();

    uint32_t start = tf_time_ms();
    while (tf_time_ms() - start < ms) {
        /* busy wait — 不阻塞中断，不产生中断 */
    }
}

void tf_delay_us(uint32_t us)
{
    if (us == 0) return;

    ensure_time_init();

    if (us >= TF_SYSTICK_PERIOD_US) {
        uint32_t wait_ms = (us + TF_SYSTICK_PERIOD_US - 1U) / TF_SYSTICK_PERIOD_US;
        uint32_t start = tf_time_ms();
        while (tf_time_ms() - start < wait_ms) {
            /* busy wait */
        }
        return;
    }

    /* 回退：NOP 循环（粗精度） */
    uint32_t n = us * ((tf_sysclk_freq_get() / 1000000UL) * TF_DELAY_COEFF) / 1000UL;
    if (n == 0) n = 1;
    while (n--) {
        __asm__ volatile("nop");
    }
}

/* ==================================================================
 *  tf_os_tick_get — OS 层时间基准
 *
 *  声明于 os/tf_os.h，实现在此（直接调用 tf_time_ms）。
 *  确保 OS 和 delay 共享同一时间基准，无 SysTick 冲突。
 * ================================================================*/

uint32_t tf_os_tick_get(void)
{
    return tf_time_ms();
}
