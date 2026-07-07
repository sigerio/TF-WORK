/**
 * @file    tf_hal_spi.c
 * @brief   GD32F10x SPI HAL 实现
 *
 * 为三路 WS2812 灯带驱动提供统一 SPI 接口：
 *   SPI0: APB2=108MHz, PSC/16 → 6.75MHz
 *   SPI1: APB1=54MHz,  PSC/8  → 6.75MHz
 *   SPI2: APB1=54MHz,  PSC/8  → 6.75MHz
 *
 * 全部配置为主模式、8bit、MSB first、软件 NSS、
 * CPOL=1 CPHA=1（Mode 3）。
 *
 * WS2812 编码字节：
 *   逻辑 0 = 0xC0 (0b11000000) → 296ns H + 888ns L
 *   逻辑 1 = 0xF0 (0b11110000) → 592ns H + 592ns L
 *
 * MCU: GD32F103RE/RC (High Density)
 * 固件库: GD32F10x Firmware Library V2.7.0
 *
 * ========== 头文件包含路径说明 ==========
 * 本文件需要编译器能够找到 gd32f10x_spi.h，
 * 已在工程选项中添加 .\GD32F10X\Include 路径。
 * =========================================
 */

#include "gd32f10x_spi.h"
#include "gd32f10x_rcu.h"
#include "../hal/tf_hal_spi.h"

/* =================================================================
 * SPI 实例识别
 * ===============================================================*/

/**
 * @brief 根据基地址获取 SPI 预分频系数
 *
 * SPI0 在 APB2 上 (108MHz)，用 /16 → 6.75MHz
 * SPI1/SPI2 在 APB1 上 (54MHz)，用 /8  → 6.75MHz
 */
static uint32_t spi_get_prescale(void *base)
{
    if (base == (void *)SPI0) {
        return SPI_PSC_16;      /* 108MHz / 16 = 6.75MHz */
    }
    /* SPI1, SPI2 */
    return SPI_PSC_8;           /* 54MHz / 8 = 6.75MHz */
}

/* =================================================================
 * 初始化
 * ===============================================================*/

bool tf_hal_spi_init(void *base)
{
    /* ---- 使能 SPI 外设时钟（必须先于 spi_init） ---- */
    if (base == (void *)SPI0) {
        rcu_periph_clock_enable(RCU_SPI0);
    } else if (base == (void *)SPI1) {
        rcu_periph_clock_enable(RCU_SPI1);
    } else {
        rcu_periph_clock_enable(RCU_SPI2);
    }

    spi_parameter_struct spi_para;

    spi_struct_para_init(&spi_para);

    spi_para.device_mode           = SPI_MASTER;
    spi_para.trans_mode            = SPI_TRANSMODE_BDTRANSMIT;  /* 只发不收 */
    spi_para.frame_size            = SPI_FRAMESIZE_8BIT;
    spi_para.nss                   = SPI_NSS_SOFT;
    spi_para.endian                = SPI_ENDIAN_MSB;
    spi_para.clock_polarity_phase  = SPI_CK_PL_HIGH_PH_2EDGE; /* CPOL=1, CPHA=1 */
    spi_para.prescale              = spi_get_prescale(base);

    spi_init((uint32_t)base, &spi_para);
    spi_enable((uint32_t)base);

    return true;
}

/* =================================================================
 * 阻塞发送
 * ===============================================================*/

void tf_hal_spi_tx(void *base, const uint8_t *data, uint32_t len)
{
    uint32_t spi = (uint32_t)base;

    for (uint32_t i = 0; i < len; i++) {
        /* 等待发送缓冲区空 */
        while (spi_i2s_flag_get(spi, SPI_FLAG_TBE) == RESET);
        spi_i2s_data_transmit(spi, data[i]);
    }
    /* 等待最后一字节发送完成（移位寄存器空） */
    while (spi_i2s_flag_get(spi, SPI_FLAG_TRANS) == SET);
}

/* =================================================================
 * 阻塞接收
 * ===============================================================*/

void tf_hal_spi_rx(void *base, uint8_t *buf, uint32_t len)
{
    uint32_t spi = (uint32_t)base;

    /* 先发送一个 dummy 字节以产生时钟 */
    for (uint32_t i = 0; i < len; i++) {
        while (spi_i2s_flag_get(spi, SPI_FLAG_TBE) == RESET);
        spi_i2s_data_transmit(spi, 0xFF);

        while (spi_i2s_flag_get(spi, SPI_FLAG_RBNE) == RESET);
        buf[i] = (uint8_t)spi_i2s_data_receive(spi);
    }
    while (spi_i2s_flag_get(spi, SPI_FLAG_TRANS) == SET);
}

/* =================================================================
 * 全双工收发
 * ===============================================================*/

void tf_hal_spi_txrx(void *base, const uint8_t *tx_data, uint8_t *rx_buf, uint32_t len)
{
    uint32_t spi = (uint32_t)base;

    for (uint32_t i = 0; i < len; i++) {
        while (spi_i2s_flag_get(spi, SPI_FLAG_TBE) == RESET);
        spi_i2s_data_transmit(spi, tx_data[i]);

        while (spi_i2s_flag_get(spi, SPI_FLAG_RBNE) == RESET);
        rx_buf[i] = (uint8_t)spi_i2s_data_receive(spi);
    }
    while (spi_i2s_flag_get(spi, SPI_FLAG_TRANS) == SET);
}

/* =================================================================
 * 状态与数据读取
 * ===============================================================*/

uint32_t tf_hal_spi_stat_get(void *base)
{
    return SPI_STAT((uint32_t)base);
}

uint8_t tf_hal_spi_data_read(void *base)
{
    return (uint8_t)spi_i2s_data_receive((uint32_t)base);
}

/* =================================================================
 * DMA 使能控制
 * ===============================================================*/

void tf_hal_spi_dma_tx_config(void *base, uint8_t enable)
{
    if (enable) {
        spi_dma_enable((uint32_t)base, SPI_DMA_TRANSMIT);
    } else {
        spi_dma_disable((uint32_t)base, SPI_DMA_TRANSMIT);
    }
}

void tf_hal_spi_dma_rx_config(void *base, uint8_t enable)
{
    if (enable) {
        spi_dma_enable((uint32_t)base, SPI_DMA_RECEIVE);
    } else {
        spi_dma_disable((uint32_t)base, SPI_DMA_RECEIVE);
    }
}

/* =================================================================
 * 中断使能控制
 * ===============================================================*/

void tf_hal_spi_interrupt_enable(void *base, uint32_t int_mask)
{
    spi_i2s_interrupt_enable((uint32_t)base, (uint8_t)int_mask);
}

void tf_hal_spi_interrupt_disable(void *base, uint32_t int_mask)
{
    spi_i2s_interrupt_disable((uint32_t)base, (uint8_t)int_mask);
}
