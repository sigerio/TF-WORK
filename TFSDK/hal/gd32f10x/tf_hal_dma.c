/**
 * @file    tf_hal_dma.c
 * @brief   GD32F10x DMA HAL 薄封装
 *
 * 基于 GD32F10x Firmware Library 的 DMA 驱动实现。
 * 当前 HAL 使用 board_dma.c 提供的方向和地址初始化 DMA 通道：
 *   - DMA0_CH2: SPI0_TX
 *   - DMA0_CH3: USART0_TX
 *   - DMA0_CH4: SPI1_TX
 *   - DMA0_CH5: USART1_RX
 *   - DMA0_CH6: USART1_TX
 *   - DMA1_CH1: SPI2_TX
 *   - DMA1_CH2: USART2_RX（常见语义 USART3_RX）
 *   - DMA1_CH4: USART2_TX（常见语义 USART3_TX）
 *
 * 通道初始地址来自 dma_config；运行时仍可通过 addr/number 接口覆盖。
 */

#include "gd32f10x.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_rcu.h"
#include "../tf_hal_dma.h"
#include "../../drv/tf_drv_dma.h"
#include "../../config/tf_board_config.h"

#if MCU_CONTAIN_PRE_DMA

static void gd32_dma_clock_enable(uint32_t periph)
{
    switch (periph) {
    case DMA0:
        rcu_periph_clock_enable(RCU_DMA0);
        break;
#ifdef DMA1
    case DMA1:
        rcu_periph_clock_enable(RCU_DMA1);
        break;
#endif
    default:
        break;
    }
}

static bool gd32_dma_default_init(const tf_drv_dma_ch_cfg_t *ch_cfg,
                                  dma_parameter_struct *init_struct)
{
    if (init_struct == NULL) {
        return false;
    }

    dma_struct_para_init(init_struct);
    init_struct->periph_addr  = (ch_cfg != NULL) ? ch_cfg->periph_addr : 0U;
    init_struct->memory_addr  = (ch_cfg != NULL) ? ch_cfg->memory_addr : 0U;
    init_struct->number       = 0U;
    init_struct->periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    init_struct->memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    init_struct->periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    init_struct->memory_width = DMA_MEMORY_WIDTH_8BIT;
    init_struct->priority     = DMA_PRIORITY_ULTRA_HIGH;

    if (ch_cfg == NULL || ch_cfg->channel > UINT8_MAX) {
        return false;
    }

    if ((ch_cfg->direction != DMA_MEMORY_TO_PERIPHERAL) &&
        (ch_cfg->direction != DMA_PERIPHERAL_TO_MEMORY)) {
        return false;
    }

    init_struct->direction = ch_cfg->direction;
    return true;
}

bool tf_hal_dma_init(const tf_drv_dma_config_t *cfg, uint8_t ch_index)
{
    if (cfg == NULL || ch_index >= E_DRV_DX_MAX) {
        return false;
    }

    const tf_drv_dma_ch_cfg_t *ch_cfg = &cfg->ch[ch_index];
    uint32_t periph = (uint32_t)cfg->base;
    dma_parameter_struct init_struct;
    dma_channel_enum channel = (dma_channel_enum)ch_cfg->channel;

    if (!gd32_dma_default_init(ch_cfg, &init_struct)) {
        return false;
    }

    gd32_dma_clock_enable(periph);

    /* 先构造通道默认模板，再执行 dma_deinit() 清硬件寄存器，最后用 init_struct 回写配置。 */
    dma_deinit(periph, channel);
    dma_init(periph, channel, &init_struct);
    dma_circulation_disable(periph, channel);
    dma_memory_to_memory_disable(periph, channel);
    dma_flag_clear(periph, channel,
                   DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    return true;
}

void tf_hal_dma_addr_config(void *base, uint8_t channel,
                            uint32_t periph_addr, uint32_t memory_addr)
{
    uint32_t periph = (uint32_t)base;

    dma_periph_address_config(periph, (dma_channel_enum)channel, periph_addr);
    dma_memory_address_config(periph, (dma_channel_enum)channel, memory_addr);
}

void tf_hal_dma_number_config(void *base, uint8_t channel, uint32_t number)
{
    dma_transfer_number_config((uint32_t)base, (dma_channel_enum)channel, number);
}

void tf_hal_dma_start(void *base, uint8_t channel)
{
    uint32_t periph = (uint32_t)base;

    dma_flag_clear(periph, (dma_channel_enum)channel,
                   DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    dma_channel_enable(periph, (dma_channel_enum)channel);
}

void tf_hal_dma_stop(void *base, uint8_t channel)
{
    dma_channel_disable((uint32_t)base, (dma_channel_enum)channel);
}

uint32_t tf_hal_dma_remaining_get(void *base, uint8_t channel)
{
    return dma_transfer_number_get((uint32_t)base, (dma_channel_enum)channel);
}

void tf_hal_dma_interrupt_config(void *base, uint8_t channel, uint32_t int_mask)
{
    dma_interrupt_enable((uint32_t)base, (dma_channel_enum)channel, int_mask);
}

uint32_t tf_hal_dma_flag_get(void *base, uint8_t channel, uint32_t flag)
{
    return (dma_flag_get((uint32_t)base, (dma_channel_enum)channel, flag) != RESET) ? flag : 0U;
}

void tf_hal_dma_flag_clear(void *base, uint8_t channel, uint32_t flag)
{
    dma_flag_clear((uint32_t)base, (dma_channel_enum)channel, flag);
}

#endif /* MCU_CONTAIN_PRE_DMA */
