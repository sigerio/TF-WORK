/**
 * @file    tf_board_spi.c
 * @brief   板级 SPI 资源配置
 *
 * 三路 SPI 全部用于 WS2812 灯带驱动：
 *   SPI0: PA5(SCK), PA7(MOSI), 无 MISO/NSS
 *   SPI1: PB13(SCK), PB15(MOSI), 无 MISO/NSS
 *   SPI2: PB3(SCK),  PB5(MOSI),  无 MISO/NSS
 *
 * MCU: GD32F103RE/RC (High Density)
 * 固件库: GD32F10x Firmware Library V2.7.0
 */

#include "tf_board_spi.h"
#include "gd32f10x.h"

#if MCU_CONTAIN_PRE_SPI
tf_drv_spi_config_t spi_configs[E_DRV_SPI_MAX] = {
    [E_DRV_SPI_0] = {
        .base = (void *)SPI0,
        .io   = { .mosi_id = E_IO_S0_MO,
                  .miso_id = E_IO_NONE,
                  .sck_id  = E_IO_S0_SCK,
                  .cs_id   = E_IO_NONE },
    },
    [E_DRV_SPI_1] = {
        .base = (void *)SPI1,
        .io   = { .mosi_id = E_IO_S1_MO,
                  .miso_id = E_IO_NONE,
                  .sck_id  = E_IO_S1_SCK,
                  .cs_id   = E_IO_NONE },
    },
    [E_DRV_SPI_2] = {
        .base = (void *)SPI2,
        .io   = { .mosi_id = E_IO_S2_MO,
                  .miso_id = E_IO_NONE,
                  .sck_id  = E_IO_S2_SCK,
                  .cs_id   = E_IO_NONE },
    },
};
#endif /* MCU_CONTAIN_PRE_SPI */
