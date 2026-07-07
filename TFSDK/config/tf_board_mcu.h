/**
 * @file    board_mcu.h
 * @brief   MCU 外设能力定义 — 板级全局配置
 *
 * 本文件是外设使能的唯一依据。
 * MCU_CONTAIN_PRE_xxx = 1 表示 MCU 具备该外设，对应 board/drv/hal 层参与编译。
 * MCU_CONTAIN_PRE_xxx = 0 表示 MCU 无此外设，整条软件栈被 #if 排除。
 *
 * 移植到不同 MCU 时只需修改此文件中的宏定义。
 *
 * ============ 编译配置说明 ============
 *
 * 在 Keil MDK 中需添加以下包含路径：
 *   .\CMSIS
 *   .\GD32F10X\Include
 *   .\TFSDK\config
 *   .\TFSDK\hal
 *   .\TFSDK\drv
 *   .\TFSDK\framework
 */

#ifndef TF_BOARD_MCU_H
#define TF_BOARD_MCU_H

/* ==================================================================
 * MCU 型号选择 — 通知 GD32F10x 固件库使用高密度定义
 *
 * GD32F103RE (512KB Flash) 和 GD32F103RC (256KB Flash) 均为 HD 器件。
 * GD32F10X_HD 必须在包含任何 GD32 库头文件之前定义。
 * 此定义也可通过 Keil 编译器 -D 宏传递，为清晰起见在此处定义。
 * ================================================================*/
 #ifndef GD32F10X_HD
 #define GD32F10X_HD
 #endif
/* ==================================================================
 * MCU 型号：GD32F103RE / GD32F103RC (High Density)
 *
 *   GD32F103RE — 512KB Flash, 64KB SRAM
 *   GD32F103RC — 256KB Flash, 48KB SRAM
 *
 *   内核: Cortex-M3 @ 72MHz
 *   GPIO: A~G (7 组)
 * ================================================================*/

/* ---- 通信接口 ---- */
#define MCU_CONTAIN_PRE_USART       1   /* USART0, USART1, USART2      */
#define MCU_CONTAIN_PRE_SPI         1   /* SPI0, SPI1, SPI2            */
#define MCU_CONTAIN_PRE_I2C         0   /* I2C0, I2C1                  */
#define MCU_CONTAIN_PRE_FDCAN       0   /* GD32F103: CAN0 (经典 CAN)   */

/* ---- 模拟 ---- */
#define MCU_CONTAIN_PRE_ADC         0   /* ADC0, ADC1, ADC2            */

/* ---- 定时器 ---- */
#define MCU_CONTAIN_PRE_TIM         0   /* TIMER — 当前工程未使用 */

/* ---- DMA ---- */
#define MCU_CONTAIN_PRE_DMA         1   /* DMA0 (7 通道)               */

/* ---- 系统 ---- */
#define MCU_CONTAIN_PRE_RTC         0   /* RTC (BKP 域)                */
#define MCU_CONTAIN_PRE_IWDG        0   /* FWDGT (独立看门狗)          */
#define MCU_CONTAIN_PRE_RCU         1   /* RCU (时钟/复位，始终使能)   */

#endif /* TF_BOARD_MCU_H */
