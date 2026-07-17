#include "tf_board_dma.h"

/* 外设数据寄存器地址（DMA 外设地址）
 * 使用 (base + offset) 纯整数运算而非 &xxx_DATA()，
 * 避免 ARMCC5 #1296-D "extended constant initialiser" 警告 */
#define USART0_DR_ADDR  ((uint32_t)(USART0 + 0x04U))
#define USART1_DR_ADDR  ((uint32_t)(USART1 + 0x04U))
#define USART2_DR_ADDR  ((uint32_t)(USART2 + 0x04U))
#define SPI0_DR_ADDR    ((uint32_t)(SPI0 + 0x0CU))
#define SPI1_DR_ADDR    ((uint32_t)(SPI1 + 0x0CU))
#define SPI2_DR_ADDR    ((uint32_t)(SPI2 + 0x0CU))

#if MCU_CONTAIN_PRE_DMA
tf_drv_dma_config_t dma_configs[E_DRV_DMA_MAX] = {
    [E_DRV_DMA_0] = {
        .base          = (void *)DMA0,
        .channel_count = E_DRV_DX_MAX,
        .ch = {
            /* SPI0_TX */
            [E_DRV_DX_CH2] = {
                .channel     = DMA_CH2,
                .periph_addr = SPI0_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
            /* USART0_TX */
            [E_DRV_DX_CH3] = {
                .channel     = DMA_CH3,
                .periph_addr = USART0_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
            /* SPI1_TX */
            [E_DRV_DX_CH4] = {
                .channel     = DMA_CH4,
                .periph_addr = SPI1_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
            /* USART1_RX */
            [E_DRV_DX_CH5] = {
                .channel     = DMA_CH5,
                .periph_addr = USART1_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_PERIPHERAL_TO_MEMORY,
                .enabled     = true,
            },
            /* USART1_TX */
            [E_DRV_DX_CH6] = {
                .channel     = DMA_CH6,
                .periph_addr = USART1_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
        },
    },
    [E_DRV_DMA_1] = {
        .base          = (void *)DMA1,
        .channel_count = E_DRV_DX_MAX,
        .ch = {
            /* SPI2_TX */
            [E_DRV_DX_CH1] = {
                .channel     = DMA_CH1,
                .periph_addr = SPI2_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
            /* GD32 USART2_RX / 常见语义 USART3_RX */
            [E_DRV_DX_CH2] = {
                .channel     = DMA_CH2,
                .periph_addr = USART2_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_PERIPHERAL_TO_MEMORY,
                .enabled     = true,
            },
            /* GD32 USART2_TX / 常见语义 USART3_TX */
            [E_DRV_DX_CH4] = {
                .channel     = DMA_CH4,
                .periph_addr = USART2_DR_ADDR,
                .memory_addr = 0U,
                .direction   = DMA_MEMORY_TO_PERIPHERAL,
                .enabled     = true,
            },
        },
    },
};
#endif /* MCU_CONTAIN_PRE_DMA */
