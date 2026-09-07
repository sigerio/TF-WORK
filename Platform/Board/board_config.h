#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#include "gd32f10x_dma.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_timer.h"

#define BOARD_USART2_ROLE_DEBUG   0U
#define BOARD_USART2_ROLE_IEC104  1U
#define BOARD_USART2_ROLE_CHARGER 2U

/* USART2 的引脚、RXNE 中断和 TX DMA 在运行时只能由一个模块占用。 */
#ifndef BOARD_USART2_ROLE
#define BOARD_USART2_ROLE BOARD_USART2_ROLE_DEBUG
#endif

#if (BOARD_USART2_ROLE != BOARD_USART2_ROLE_DEBUG) && \
    (BOARD_USART2_ROLE != BOARD_USART2_ROLE_IEC104) && \
    (BOARD_USART2_ROLE != BOARD_USART2_ROLE_CHARGER)
#error BOARD_USART2_ROLE must select DEBUG, IEC104, or CHARGER
#endif

/*
 * 巡检机巢 底层 IO 拓扑配置
 *
 * 数据来源: datasheet/巡检机巢/IO拓扑.md
 * 本文件只描述硬件资源与引脚映射(拓扑), 不包含任何业务逻辑。
 *
 * 需要 JTAG 释放的引脚 (GPIO_SWJ_SWDPENABLE_REMAP, 关闭 JTAG 保留 SWD):
 *   PA15(空调485方向) PB3(无刷485方向)
 *
 * 说明:
 *   - 装饰灯带使用 SPI0 默认映射：PA5=SCK、PA7=MOSI。
 *     PA5 与平台收回到位检测分时复用：WS2812 DMA 发送期间为 SPI0_SCK，
 *     发送完成后恢复为平台检测输入；PB3 始终为 UART4 485 方向控制。
 */

typedef enum {
    /* ---- 串口引脚 ---- */
    BOARD_PIN_USART0_TX = 0,   /* PA9  - 温湿度485 */
    BOARD_PIN_USART0_RX,       /* PA10 - 温湿度485 */
    BOARD_PIN_USART1_TX,       /* PA2  - 主板通信 后续复用4G*/
    BOARD_PIN_USART1_RX,       /* PA3  - 主板通信 后续复用4G*/
    BOARD_PIN_USART2_TX,       /* PB10 - 风速485 */
    BOARD_PIN_USART2_RX,       /* PB11 - 风速485 */
    BOARD_PIN_USART3_TX,       /* PC10 - 雨量485   当前复用 水浸485*/
    BOARD_PIN_USART3_RX,       /* PC11 - 雨量485   当前复用 水浸485*/
    // BOARD_PIN_UART4_TX,        /* PC12 - 水浸485 */
    // BOARD_PIN_UART4_RX,        /* PD2  - 水浸485 */
    
    /* ---- 485 方向控制 (拉高发送/拉低接收, 默认拉低) ---- */
    BOARD_PIN_RS485_MAIN_DIR,      /* PA1  - 主板 */
    BOARD_PIN_RS485_WIND_DIR,   /* PB2  - 风速 */
    BOARD_PIN_RS485_RAIN_DIR,        /* PA15 - 雨量 */
    BOARD_PIN_RS485_TC_DIR, /* PA8  - 温湿度 */

    
    /* ---- 其它输出 (拉低触发, 默认拉高) ---- */

    BOARD_PIN_LED,     /* PC7 - 指示灯 */
    BOARD_PIN_BEEP,      /* PC1 - 蜂鸣器 */

    BOARD_PIN_COUNT,
    BOARD_PIN_NONE = -1,
} board_pin_id_e;

typedef enum {
    BOARD_USART_TC = 0, /* USART0 - 温湿度485 */
    BOARD_USART_MAIN,   /* USART1 - 主板通信 */
    BOARD_USART_WIND,     /* USART2 - 风速通信 */
    BOARD_USART_RAIN,          /* USART3 - 雨量485 */
    // BOARD_USART_WATER,   /* UART4  - 水浸485 */
    BOARD_USART_COUNT,
} board_usart_id_e;

typedef enum {
    BOARD_SPI_WS2812 = 0, /* SPI0 默认映射 - 装饰灯带（SCK=PA5，MOSI=PA7） */
    BOARD_SPI_COUNT,
} board_spi_id_e;

typedef enum {
    BOARD_PWM_PLATFORM = 0, /* TIMER1 - 平台推杆电机 */
    BOARD_PWM_DOOR,         /* TIMER3 - 门电机1/2 */
    BOARD_PWM_COUNT,
} board_pwm_id_e;

/* TIMER 通道 id（ch id）：作为通道数组索引，链接 通道实例 - io id */
typedef enum {
    BOARD_PWM_CH0 = 0,
    BOARD_PWM_CH1,
    BOARD_PWM_CH2,
    BOARD_PWM_CH3,
    BOARD_PWM_CH_COUNT,
} board_pwm_ch_id_e;

/* DMA 通道 id（ch id）：作为通道数组索引，链接 通道实例 - io id */
typedef enum {
    BOARD_DMA_CH0 = 0,
    BOARD_DMA_CH1,
    BOARD_DMA_CH2,
    BOARD_DMA_CH3,
    BOARD_DMA_CH4,
    BOARD_DMA_CH5,
    BOARD_DMA_CH6,
    BOARD_DMA_CH_COUNT,
} board_dma_ch_id_e;

/* DMA 控制器 id */
typedef enum {
    BOARD_DMA_0 = 0,
    BOARD_DMA_COUNT,
} board_dma_id_e;

typedef struct {
    uint32_t port;
    uint32_t pin;
    uint32_t mode;
    uint32_t speed;
    uint8_t initial_level;
} board_gpio_config_t;

/* DMA 通道项：由 ch id 索引，含 通道实例 + 外设地址 + 关联 io id */
typedef struct {
    dma_channel_enum channel;               /* 通道实例 DMA_CH0..DMA_CH6 */
    uint32_t peripheral_address;            /* 外设数据寄存器地址（0 表示未用） */
    uint32_t direction;                     /* DMA_PERIPHERAL_TO_MEMORY / DMA_MEMORY_TO_PERIPHERAL */
    board_pin_id_e pin_id;                  /* 关联 IO（BOARD_PIN_NONE 表示无） */
} board_dma_channel_config_t;

/* DMA 控制器：基址 + 通道链（ch id -> (通道实例, io id)） */
typedef struct {
    uint32_t controller;                                /* DMA0 / DMA1 */
    rcu_periph_enum clock;                              /* RCU_DMA0 / RCU_DMA1 */
    board_dma_channel_config_t ch[BOARD_DMA_CH_COUNT];  /* ch id - io id 链 */
} board_dma_config_t;

/* DMA 通道引用：dma 控制器 id + ch id（BOARD_DMA_ID_NONE 表示无 DMA） */
typedef struct {
    board_dma_id_e dma;         /* DMA 控制器（BOARD_DMA_0 / BOARD_DMA_1） */
    board_dma_ch_id_e ch;       /* 通道 id */
} board_dma_ref_t;

#define BOARD_DMA_REF_NONE { BOARD_DMA_COUNT, BOARD_DMA_CH_COUNT }

/* 外设 - (dma + io)：dma 与 io 平级，均由外设直接持有 */
typedef struct {
    uint32_t peripheral;
    rcu_periph_enum clock;
    board_pin_id_e tx_pin;                  /* TX IO（BOARD_PIN_NONE 表示无） */
    board_pin_id_e rx_pin;                  /* RX IO（BOARD_PIN_NONE 表示无） */
    board_dma_ref_t rx_dma;                 /* RX DMA（BOARD_DMA_REF_NONE 表示无） */
    board_dma_ref_t tx_dma;                 /* TX DMA（BOARD_DMA_REF_NONE 表示无） */
    board_pin_id_e direction_pin;           /* 485 方向脚（BOARD_PIN_NONE 表示非 485） */
    uint32_t baudrate;
} board_usart_config_t;

typedef struct {
    uint32_t peripheral;
    rcu_periph_enum clock;
    board_pin_id_e sck_pin;                 /* SCK IO */
    board_pin_id_e mosi_pin;                /* MOSI IO */
    board_dma_ref_t tx_dma;                 /* TX DMA */
    uint32_t prescale;
} board_spi_config_t;

/* TIMER 输出通道项：由 ch id 索引，含 通道实例 + io id */
typedef struct {
    uint16_t channel;           /* 通道实例 TIMER_CH_0..TIMER_CH_3 */
    board_pin_id_e pin_id;      /* 该通道输出到的 IO（BOARD_PIN_NONE 表示未用） */
} board_pwm_channel_config_t;

/* TIMER 初始配置 + 通道链（ch id -> (通道实例, io id)） */
typedef struct {
    uint32_t peripheral;      /* TIMER1 / TIMER3 */
    rcu_periph_enum clock;
    uint32_t prescaler;       /* 计数时钟 = 108MHz/(prescaler+1) */
    uint32_t period;          /* PWM 周期 (默认99%高电平 = 0.99*(period+1)) */
    board_pwm_channel_config_t ch[BOARD_PWM_CH_COUNT];  /* ch id - io id 链 */
} board_pwm_config_t;

extern const board_gpio_config_t board_gpio_configs[BOARD_PIN_COUNT];
extern const board_usart_config_t board_usart_configs[BOARD_USART_COUNT];
extern const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT];
extern const board_spi_config_t board_spi_configs[BOARD_SPI_COUNT];
extern const board_pwm_config_t board_pwm_configs[BOARD_PWM_COUNT];

void board_init(void);
void board_gpio_init(board_pin_id_e pin_id);

/* 解析 dma 引用 -> 通道配置（dma-ch id-io id 链） */
#define BOARD_DMA_CH(ref) (&board_dma_configs[(ref).dma].ch[(ref).ch])
#define BOARD_DMA_HAS(ref) ((ref).dma < BOARD_DMA_COUNT)

/* 便于上层按 ID 直接读写引脚电平（含 gpio_bit_read/gpio_bit_write 的参数展开） */
#define BOARD_PIN_PORT(id)   (board_gpio_configs[id].port)
#define BOARD_PIN_PIN(id)    (board_gpio_configs[id].pin)

#endif /* BOARD_CONFIG_H */
