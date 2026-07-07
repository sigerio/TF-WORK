/**
 * @file    tf_drv_timer.c
 * @brief   Timer 外设注册与通道操作
 *
 * 标准 Timer 通道初始化流程（平台无关）：
 *
 *   register: 物理定时器注册
 *   ch_config_default: 显式应用 board config 默认通道配置
 *   ch_config: 配置通道模式与初始参数
 *   ch_start: 使能该通道功能输出
 *   timer_start: 启动父定时器计数
 *   set_duty: 更新比较值
 */

#include "tf_drv_timer.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_timer.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_TIM

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif

/* ========== 运行时状态 ========== */

static bool  timer_registered[E_DRV_TIMER_MAX];
static uint32_t timer_ch_started_mask[E_DRV_TIMER_MAX];  /* bitmask: bit N → ch[N] started */

/* ========== 内部辅助 ========== */

static uint8_t timer_channel_capacity(const tf_drv_timer_config_t *cfg)
{
    if (cfg == NULL) {
        return E_DRV_T2_CH_MAX;
    }

    if (cfg->channel_count == 0 || cfg->channel_count > E_DRV_T2_CH_MAX) {
        return E_DRV_T2_CH_MAX;
    }

    return cfg->channel_count;
}

static int timer_get_id(tf_drv_timer_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_timer_config_t *)h - timer_configs;
    if (id < 0 || id >= E_DRV_TIMER_MAX || !timer_registered[id])
        return -1;
    return id;
}

static void timer_apply_channel_config(int id, uint8_t ch_idx)
{
    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    const tf_drv_timer_ch_cfg_t *ch = &cfg->ch[ch_idx];
    void *tim_base = cfg->tim;

    if (ch->enabled) {
        tf_hal_timer_ch_pwm_mode_config(tim_base, ch->channel);
        TF_DEBUG("HAL", "  ch_pwm_config(tim=%p, ch=%d, hw_ch=%lu)\n",
                 tim_base, ch_idx, (unsigned long)ch->channel);
    }
}

/* ========== 对外接口实现 ========== */

tf_drv_timer_handle_t tf_drv_timer_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_TIMER_MAX);
    if (id < 0 || id >= E_DRV_TIMER_MAX) {
        return NULL;
    }

    if (timer_registered[id]) {
        return (tf_drv_timer_handle_t)&timer_configs[id];
    }

    const tf_drv_timer_config_t *cfg = &timer_configs[id];

    if (cfg->tim == NULL) {
        return NULL;
    }

    tf_hal_timer_init(cfg);

    uint8_t ch_count = timer_channel_capacity(cfg);
    for (uint8_t ch = 0; ch < ch_count; ch++) {
        if (cfg->ch[ch].enabled) {
            if (cfg->ch[ch].io_id >= 0) {
                tf_drv_io_register(cfg->ch[ch].io_id);
            }
            timer_apply_channel_config(id, ch);
        }
    }

    timer_registered[id] = true;

    return (tf_drv_timer_handle_t)&timer_configs[id];
}

void tf_drv_timer_start(tf_drv_timer_handle_t h)
{
    int id = timer_get_id(h);
    if (id < 0) return;

    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    tf_hal_timer_start(cfg->tim);
}

void tf_drv_timer_stop(tf_drv_timer_handle_t h)
{
    int id = timer_get_id(h);
    if (id < 0) return;

    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    tf_hal_timer_stop(cfg->tim);
}

tf_drv_timer_handle_t tf_drv_timer_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_TIMER_MAX) {
        return NULL;
    }

    if (!timer_registered[id]) {
        return NULL;
    }

    return (tf_drv_timer_handle_t)&timer_configs[id];
}

uint32_t tf_drv_timer_get_tick(tf_drv_timer_handle_t h)
{
    int id = timer_get_id(h);
    if (id < 0) return 0;
    void *base = timer_configs[id].tim;
    return base ? tf_hal_timer_get_cnt(base) : 0;
}

void tf_drv_timer_ch_start(tf_drv_timer_handle_t h, uint32_t ch)
{
    int id = timer_get_id(h);
    if (id < 0) return;

    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    int ch_idx = (int)ch;
    if (ch_idx < 0 || ch_idx >= timer_channel_capacity(cfg)) return;

    const tf_drv_timer_ch_cfg_t *ch_cfg = &cfg->ch[ch_idx];
    if (!ch_cfg->enabled) return;

    tf_hal_timer_ch_output_enable(cfg->tim, ch_cfg->channel);
    timer_ch_started_mask[id] |= (1u << ch_idx);
}

void tf_drv_timer_ch_stop(tf_drv_timer_handle_t h, uint32_t ch)
{
    int id = timer_get_id(h);
    if (id < 0) return;

    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    int ch_idx = (int)ch;
    if (ch_idx < 0 || ch_idx >= timer_channel_capacity(cfg)) return;

    const tf_drv_timer_ch_cfg_t *ch_cfg = &cfg->ch[ch_idx];
    if (!ch_cfg->enabled) return;

    tf_hal_timer_ch_output_disable(cfg->tim, ch_cfg->channel);
    timer_ch_started_mask[id] &= ~(1u << ch_idx);
}

void tf_drv_timer_ch_pwm_set_duty(tf_drv_timer_handle_t h, uint32_t ch, uint8_t duty)
{
    int id = timer_get_id(h);
    if (id < 0) return;

    const tf_drv_timer_config_t *cfg = &timer_configs[id];
    int ch_idx = (int)ch;
    if (ch_idx < 0 || ch_idx >= timer_channel_capacity(cfg)) return;

    const tf_drv_timer_ch_cfg_t *ch_cfg = &cfg->ch[ch_idx];
    if (!ch_cfg->enabled) return;

    tf_hal_timer_ch_pwm_set_duty(cfg->tim, ch_cfg->channel, duty);
}
#endif /* MCU_CONTAIN_PRE_TIM */
