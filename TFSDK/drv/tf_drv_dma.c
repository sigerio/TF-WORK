/**
 * @file    tf_drv_dma.c
 * @brief   DMA 外设注册与通道传输控制实现
 *
 * 标准 DMA 控制器通道初始化流程（平台无关）：
 *
 *   register(id):
 *     1. 校验 base != NULL
 *     2. 遍历 ch[] → 逐个调用 hal_dma_init()
 *     3. 标记 registered
 */

#include "tf_drv_dma.h"
#include "../hal/tf_hal_dma.h"
#include "../framework/tf_debug.h"

#if MCU_CONTAIN_PRE_DMA


/* ========== 运行时状态 ========== */

extern tf_drv_dma_config_t dma_configs[];

static bool dma_registered[E_DRV_DMA_MAX];

/* ========== 内部辅助 ========== */

/**
 * @brief 获取有效通道数量
 *
 * channel_count == 0 表示使用当前资源枚举上限 E_DRV_DX_MAX。
 */
static uint8_t dma_channel_capacity(const tf_drv_dma_config_t *cfg)
{
    if (cfg == NULL) {
        return E_DRV_DX_MAX;
    }
    if (cfg->channel_count == 0 || cfg->channel_count > E_DRV_DX_MAX) {
        return E_DRV_DX_MAX;
    }
    return cfg->channel_count;
}

static int dma_get_id(tf_drv_dma_handle_t h)
{
    if (h == NULL) return -1;
    int id = (tf_drv_dma_config_t *)h - dma_configs;
    if (id < 0 || id >= E_DRV_DMA_MAX || !dma_registered[id])
        return -1;
    return id;
}

/* ========== HAL 封装（static，加 DEBUG 追踪） ========== */

static bool hal_dma_init(const tf_drv_dma_config_t *cfg, uint8_t ch_index)
{
    return tf_hal_dma_init(cfg, ch_index);
}

static bool dma_get_hw_channel(const tf_drv_dma_config_t *cfg, uint8_t ch_index, uint8_t *channel)
{
    if (cfg == NULL || channel == NULL) {
        return false;
    }
    if (ch_index >= dma_channel_capacity(cfg) || !cfg->ch[ch_index].enabled) {
        return false;
    }
    if (cfg->ch[ch_index].channel > UINT8_MAX) {
        return false;
    }

    *channel = (uint8_t)cfg->ch[ch_index].channel;
    return true;
}

/* ========== 对外函数实现 ========== */

tf_drv_dma_handle_t tf_drv_dma_register(int id)
{

    if (id < 0 || id >= E_DRV_DMA_MAX) {
        return NULL;
    }

    if (dma_registered[id]) {
        return (tf_drv_dma_handle_t)&dma_configs[id];
    }

    const tf_drv_dma_config_t *cfg = &dma_configs[id];

    if (cfg->base == NULL) {
        return NULL;
    }


    /* ---- 遍历 ch[]，逐个绑定/初始化（具体参数由 CubeMX/Board Port 提供） ---- */
    uint8_t ch_count = dma_channel_capacity(cfg);
    for (uint8_t i = 0; i < ch_count; i++) {
        if (cfg->ch[i].enabled) {

            if (!hal_dma_init(cfg, i)) {
                return NULL;
            }
        }
    }

    dma_registered[id] = true;

    return (tf_drv_dma_handle_t)&dma_configs[id];
}

tf_drv_dma_handle_t tf_drv_dma_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_DMA_MAX) {
        return NULL;
    }

    if (!dma_registered[id]) {
        return NULL;
    }

    return (tf_drv_dma_handle_t)&dma_configs[id];
}

bool tf_drv_dma_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_DMA_MAX)
                ? dma_registered[id] : false;
    return ret;
}

/* ========== 通道运行时接口实现 ========== */

void tf_drv_dma_start(tf_drv_dma_handle_t h, uint8_t ch_index)
{
    int id = dma_get_id(h);
    if (id < 0) return;
    const tf_drv_dma_config_t *cfg = &dma_configs[id];
    uint8_t channel;
    if (!dma_get_hw_channel(cfg, ch_index, &channel)) return;
    tf_hal_dma_start(cfg->base, channel);
}

void tf_drv_dma_stop(tf_drv_dma_handle_t h, uint8_t ch_index)
{
    int id = dma_get_id(h);
    if (id < 0) return;
    const tf_drv_dma_config_t *cfg = &dma_configs[id];
    uint8_t channel;
    if (!dma_get_hw_channel(cfg, ch_index, &channel)) return;
    tf_hal_dma_stop(cfg->base, channel);
}

void tf_drv_dma_addr_config(tf_drv_dma_handle_t h, uint8_t ch_index,
                             uint32_t periph_addr, uint32_t memory_addr)
{
    int id = dma_get_id(h);
    if (id < 0) return;
    tf_drv_dma_config_t *cfg = &dma_configs[id];
    uint8_t channel;
    if (!dma_get_hw_channel(cfg, ch_index, &channel)) return;

    /* 同步更新 config 条目，保持与硬件寄存器一致 */
    cfg->ch[ch_index].periph_addr = periph_addr;
    cfg->ch[ch_index].memory_addr = memory_addr;

    tf_hal_dma_addr_config(cfg->base, channel, periph_addr, memory_addr);
}

void tf_drv_dma_number_config(tf_drv_dma_handle_t h, uint8_t ch_index,
                               uint32_t number)
{
    int id = dma_get_id(h);
    if (id < 0) return;
    const tf_drv_dma_config_t *cfg = &dma_configs[id];
    uint8_t channel;
    if (!dma_get_hw_channel(cfg, ch_index, &channel)) return;
    tf_hal_dma_number_config(cfg->base, channel, number);
}

uint32_t tf_drv_dma_remaining_get(tf_drv_dma_handle_t h, uint8_t ch_index)
{
    int id = dma_get_id(h);
    if (id < 0) return 0;
    const tf_drv_dma_config_t *cfg = &dma_configs[id];
    uint8_t channel;
    if (!dma_get_hw_channel(cfg, ch_index, &channel)) return 0;
    return tf_hal_dma_remaining_get(cfg->base, channel);
}
#endif /* MCU_CONTAIN_PRE_DMA */
