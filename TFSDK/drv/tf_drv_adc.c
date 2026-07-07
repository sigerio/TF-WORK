/**
 * @file    tf_drv_adc.c
 * @brief   ADC 外设配置与转换控制实现
 *
 * 实现要点：
 *   - 用户必须提供 adc_configs[] 数组（命名固定）
 *   - 所有硬件操作委托给 tf_hal_adc.h 中的内联函数
 *   - 内部函数均为 static
 */

#include "tf_drv_adc.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_adc.h"
#include "../framework/tf_debug.h"
#include "../framework/tf_delay.h"
#if MCU_CONTAIN_PRE_ADC

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif

/* ========== 运行时状态 ========== */

static bool adc_registered[E_DRV_ADC_MAX];

static void adc_register_channel_ios(int id)
{
    const tf_drv_adc_config_t *cfg = &adc_configs[id];
    for (uint8_t i = 0; i < cfg->ch_count && i < TF_DRV_ADC_CFG_CH_CAPACITY; i++) {
        if (cfg->ch[i].io_id >= 0) {
            tf_drv_io_register(cfg->ch[i].io_id);
        }
    }
}

static void hal_init_common(int id)
{
    const tf_drv_adc_config_t *c = &adc_configs[id];

    tf_hal_adc_init(c->base);

    /* 使能后等待电压稳定，再校准 */
    tf_delay_ms(1);
    tf_hal_adc_calibration_enable(c->base);
}

/* ========== 内部辅助 ========== */

static int adc_get_id(tf_drv_adc_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_adc_config_t *)h - adc_configs;
    if (id < 0 || id >= E_DRV_ADC_MAX || !adc_registered[id])
        return -1;
    return id;
}

static void *adc_get_base(tf_drv_adc_handle_t h)
{
    int id = adc_get_id(h);
    return (id >= 0) ? adc_configs[id].base : NULL;
}

/* ========== 对外函数实现 ========== */

tf_drv_adc_handle_t tf_drv_adc_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_ADC_MAX);
    if (id < 0 || id >= E_DRV_ADC_MAX) {
        return NULL;
    }

    if (adc_registered[id]) {
        return (tf_drv_adc_handle_t)&adc_configs[id];
    }

    const tf_drv_adc_config_t *cfg = &adc_configs[id];

    if (cfg->base == NULL) {
        return NULL;
    }

             cfg->base, cfg->ch_count);

    adc_register_channel_ios(id);
    hal_init_common(id);

    adc_registered[id] = true;

    return (tf_drv_adc_handle_t)&adc_configs[id];
}

tf_drv_adc_handle_t tf_drv_adc_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_ADC_MAX) {
        return NULL;
    }

    if (!adc_registered[id]) {
        return NULL;
    }

    return (tf_drv_adc_handle_t)&adc_configs[id];
}

bool tf_drv_adc_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_ADC_MAX) ? adc_registered[id] : false;
    return ret;
}

/* ---- 标志位 ---- */

uint32_t tf_drv_adc_flag_get(tf_drv_adc_handle_t h, uint32_t flag)
{
    int id = adc_get_id(h);
    if (id < 0) return 0;
    return tf_hal_adc_flag_get(adc_configs[id].base, flag);
}

void tf_drv_adc_flag_clear(tf_drv_adc_handle_t h, uint32_t flag)
{
    int id = adc_get_id(h);
    if (id < 0) return;
    tf_hal_adc_flag_clear(adc_configs[id].base, flag);
}

void tf_drv_adc_software_trigger_enable(tf_drv_adc_handle_t h, uint8_t group)
{
    void *base = adc_get_base(h);
    if (base) tf_hal_adc_software_trigger_enable(base, group);
}

uint16_t tf_drv_adc_regular_data_read(tf_drv_adc_handle_t h)
{
    void *base = adc_get_base(h);
    return base ? tf_hal_adc_regular_data_read(base) : 0;
}

uint16_t tf_drv_adc_inserted_data_read(tf_drv_adc_handle_t h, uint8_t channel)
{
    void *base = adc_get_base(h);
    return base ? tf_hal_adc_inserted_data_read(base, channel) : 0;
}
#endif /* MCU_CONTAIN_PRE_ADC */
