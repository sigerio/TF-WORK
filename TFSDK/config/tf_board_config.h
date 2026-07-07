/**
 * @file    board_config.h
 * @brief   板级配置 — 当前工程正在使用的资源枚举
 *
 * 本文件只描述当前工程实际启用的资源枚举，不描述 MCU 持有的全量资源容量。
 * 驱动层需要的数组尺寸应直接基于这些资源枚举推导，不在此处额外定义静态数量宏。
 *
 * 新增资源时只在此添加对应枚举。
 *
 * ============ MCU 配置说明 ============
 *
 * 本工程目标 MCU: GD32F103RE / GD32F103RC (High Density)
 * 需要在编译选项中定义 GD32F10X_HD（已在 CMSIS/gd32f10x.h 中启用）。
 *
 * 外设命名对应关系（GD32 → STM32 习惯）：
 *   USART0 → USART1, USART1 → USART2, USART2 → USART3
 *   SPI0   → SPI1,   SPI1   → SPI2
 *   TIMER0 → TIM1,   TIMER1 → TIM2,   TIMER2 → TIM3,   ...
 *
 * 当前 UART 枚举按 GD32 外设编号组织（UART_0/1/2 -> USART0/1/2），
 * 其他外设若保留抽象命名，则在注释中标注 GD32 外设名。
 */

#ifndef TF_BOARD_CONFIG_H
#define TF_BOARD_CONFIG_H

/* MCU 外设能力定义 — 控制 board/drv/hal 层条件编译 */
#include "tf_board_mcu.h"

/* 厂商硬件宏定义 — GD32F10x 固件库头文件 */
/* 注意：需在编译选项中添加 CMSIS/ 和 GD32F10X/Include/ 到包含路径 */
#include "gd32f10x.h"         /* 间接包含 gd32f10x_gpio.h, gd32f10x_rcu.h 等 */

/* ========== IO ========== */

typedef enum {
    E_IO_NONE = -1,

    /* ── USART ── */
    E_IO_U0_TX,         /* PA9  — USART0_TX   */
    E_IO_U0_RX,         /* PA10 — USART0_RX   */
    E_IO_U1_TX,         /* PA2  — USART1_TX   */
    E_IO_U1_RX,         /* PA3  — USART1_RX   */
    E_IO_U2_TX,         /* PB10 — GD32 USART2_TX / 常见语义 USART3_TX */
    E_IO_U2_RX,         /* PB11 — GD32 USART2_RX / 常见语义 USART3_RX */

    /* ── SPI0 (APB2) — 默认引脚 PA5(SCK), PA7(MOSI) ── */
    E_IO_S0_SCK,        /* PA5  — SPI0_SCK  */
    E_IO_S0_MO,         /* PA7  — SPI0_MOSI */

    /* ── SPI1 (APB1) — 默认引脚 PB13(SCK), PB15(MOSI) ── */
    E_IO_S1_SCK,        /* PB13 — SPI1_SCK  */
    E_IO_S1_MO,         /* PB15 — SPI1_MOSI */

    /* ── SPI2 (APB1) — 默认引脚 PB3(SCK), PB5(MOSI) ── */
    E_IO_S2_SCK,        /* PB3  — SPI2_SCK  */
    E_IO_S2_MO,         /* PB5  — SPI2_MOSI */

    E_IO_MAX
} tf_drv_io_id_e;

/* ========== Timer ========== */
/* 当前工程未使用 Timer */

//typedef enum {
//    E_DRV_TIMER_NONE = -1,
//    E_DRV_TIMER_2,
//    E_DRV_TIMER_MAX
//} tf_drv_timer_id_e;
//
//typedef enum {
//    E_DRV_T2_CH_NONE = -1,
//    E_DRV_T2_CH1,
//    E_DRV_T2_CH_MAX
//} tf_drv_timer_ch_id_e;

/* ========== UART ========== */

/* GD32F103 对应外设：
 *   UART_0 → USART0 (PA9/PA10)
 *   UART_1 → USART1 (PA2/PA3)
 *   UART_2 → GD32 USART2 / 常见语义 USART3 (PB10/PB11)
 */
typedef enum {
    E_DRV_UART_NONE = -1,
    E_DRV_UART_0,               /* USART0: PA9(TX),  PA10(RX)  */
    E_DRV_UART_1,               /* USART1: PA2(TX),  PA3(RX)   */
    E_DRV_UART_2,               /* GD32 USART2 / 常见语义 USART3: PB10(TX), PB11(RX) */
    E_DRV_UART_MAX
} tf_drv_uart_id_e;

/* ========== SPI ========== */

typedef enum {
    E_DRV_SPI_NONE = -1,
    E_DRV_SPI_0,
    E_DRV_SPI_1,
    E_DRV_SPI_2,
    E_DRV_SPI_MAX
} tf_drv_spi_id_e;

/* ========== I2C ========== */

typedef enum {
    E_DRV_I2C_NONE = -1,
    E_DRV_I2C_0,
    E_DRV_I2C_1,
    E_DRV_I2C_MAX
} tf_drv_i2c_id_e;

/* ========== ADC ========== */

typedef enum {
    E_DRV_ADC_NONE = -1,
    E_DRV_ADC_0,
    E_DRV_ADC_1,
    E_DRV_ADC_MAX
} tf_drv_adc_id_e;

/* ========== CAN ========== */

typedef enum {
    E_DRV_CAN_NONE = -1,
    E_DRV_CAN_0,
    E_DRV_CAN_MAX
} tf_drv_can_id_e;

/* ========== DMA ========== */

typedef enum {
    E_DRV_DMA_NONE = -1,
    E_DRV_DMA_0,
    E_DRV_DMA_1,
    E_DRV_DMA_MAX
} tf_drv_dma_id_e;

typedef enum {
    E_DRV_D0_NONE = -1,
    E_DRV_D0_CH1,
    E_DRV_D0_CH2,
    E_DRV_D0_CH3,
    E_DRV_D0_CH4,
    E_DRV_D0_CH5,
    E_DRV_D0_CH6,
    E_DRV_D0_MAX
} tf_drv_dma_ch_id_e;

/* ========== RTC ========== */

typedef enum {
    E_DRV_RTC_NONE = -1,
    E_DRV_RTC_0,
    E_DRV_RTC_MAX
} tf_drv_rtc_id_e;

/* ========== FWDGT ========== */

typedef enum {
    E_DRV_FWDGT_NONE = -1,
    E_DRV_FWDGT_0,
    E_DRV_FWDGT_MAX
} tf_drv_fwdgt_id_e;

#endif /* TF_BOARD_CONFIG_H */
