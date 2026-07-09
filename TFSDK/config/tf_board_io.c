/**
 * @file    tf_board_io.c
 * @brief   板级 IO 引脚配置数组
 *
 * 当前工程使用三路 SPI 驱动 WS2812 灯带：
 *   SPI0: PA5(SCK), PA7(MOSI)
 *   SPI1: PB13(SCK), PB15(MOSI)
 *   SPI2: PB3(SCK),  PB5(MOSI)
 *
 * 所有 SPI 不需要 CS/MISO 引脚。
 *
 * MCU: GD32F103RE/RC (High Density)
 * 固件库: GD32F10x Firmware Library V2.7.0
 */

#include "tf_board_io.h"
#include "gd32f10x.h"       /* GPIO 端口/引脚宏 */
#include "gd32f10x_gpio.h"  /* GPIO_MODE_xxx, GPIO_OSPEED_xxx */

tf_drv_io_config_t io_configs[E_IO_MAX] = {

    /* =================================================================
     * USART0 — PA9(TX), PA10(RX)
     * ===============================================================*/
    [E_IO_U0_TX] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_9,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_U0_RX] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_10,
        .mode       = GPIO_MODE_IN_FLOATING,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    /* =================================================================
     * USART1 — PA2(TX), PA3(RX)
     * ===============================================================*/
    [E_IO_U1_TX] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_2,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_U1_RX] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_3,
        .mode       = GPIO_MODE_IN_FLOATING,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    /* =================================================================
     * USART2 — PB10(TX), PB11(RX)
     * ===============================================================*/
    [E_IO_U2_TX] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_10,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_U2_RX] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_11,
        .mode       = GPIO_MODE_IN_FLOATING,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    /* =================================================================
     * SPI0 — PA5(SCK), PA7(MOSI)
     * 默认引脚，无需重映射。
     * ===============================================================*/
    [E_IO_S0_SCK] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_5,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_S0_MO] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_7,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    /* =================================================================
     * SPI1 — PB13(SCK), PB15(MOSI)
     * 默认引脚，无需重映射。
     * ===============================================================*/
    [E_IO_S1_SCK] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_13,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_S1_MO] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_15,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    /* =================================================================
     * SPI2 — PB3(SCK), PB5(MOSI)
     * 默认引脚，无需重映射（HD 器件不支持 SPI2 重映射）。
     * ===============================================================*/
    [E_IO_S2_SCK] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_3,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_S2_MO] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_5,
        .mode       = GPIO_MODE_AF_PP,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    [E_IO_485_CTRL] = {
        .port       = (uintptr_t)GPIOA,
        .pin        = GPIO_PIN_11,
        .mode       = GPIO_MODE_OUT_PP,
        .speed      = GPIO_OSPEED_50MHZ,
        .init_state = 1U,
    },

    [E_IO_KEY_HELIPED] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_7,
        .mode       = GPIO_MODE_IPD,
        .speed      = GPIO_OSPEED_50MHZ,
    },

    [E_IO_KEY_DOOR] = {
        .port       = (uintptr_t)GPIOB,
        .pin        = GPIO_PIN_6,
        .mode       = GPIO_MODE_IPD,
        .speed      = GPIO_OSPEED_50MHZ,
    },
    [E_IO_KEY_EM_STOP] = {
        .port       = (uintptr_t)GPIOC,
        .pin        = GPIO_PIN_10,
        .mode       = GPIO_MODE_IPD,
        .speed      = GPIO_OSPEED_50MHZ,
    },

};
