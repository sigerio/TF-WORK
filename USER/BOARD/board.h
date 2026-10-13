#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

#include "gd32f10x_dma.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_timer.h"

#define BOARD_USART2_ROLE_DEBUG   0U
#define BOARD_USART2_ROLE_IEC104  1U
#define BOARD_USART2_ROLE_CHARGER 2U

/* 485 方向控制经低速光耦隔离，切到发送后留出建立时间，可按实测波形调整。 */
#ifndef BOARD_RS485_TX_SETUP_US
#define BOARD_RS485_TX_SETUP_US 100U
#endif

/* USART2 的引脚、RXNE 中断和 TX DMA 在运行时只能由一个模块占用。 */
#ifndef BOARD_USART2_ROLE
#define BOARD_USART2_ROLE BOARD_USART2_ROLE_DEBUG
#endif

#if (BOARD_USART2_ROLE != BOARD_USART2_ROLE_DEBUG) && (BOARD_USART2_ROLE != BOARD_USART2_ROLE_IEC104) && (BOARD_USART2_ROLE != BOARD_USART2_ROLE_CHARGER)
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
    BOARD_PIN_USART0_TX = 0,   /* PA9  - 气象板485 */
    BOARD_PIN_USART0_RX,       /* PA10 - 气象板485 */
    BOARD_PIN_USART1_TX,       /* PA2  - 主板通信 */
    BOARD_PIN_USART1_RX,       /* PA3  - 主板通信 */
    BOARD_PIN_USART2_TX,       /* PB10 - 充电板通信 */
    BOARD_PIN_USART2_RX,       /* PB11 - 充电板通信 */
    BOARD_PIN_USART3_TX,       /* PC10 - 空调485 */
    BOARD_PIN_USART3_RX,       /* PC11 - 空调485 */
    BOARD_PIN_UART4_TX,        /* PC12 - 无刷电机485 */
    BOARD_PIN_UART4_RX,        /* PD2  - 无刷电机485 */
    /* ---- 灯带 SPI0 ---- */
    BOARD_PIN_SPI0_SCK,        /* PA5  - SPI0 时钟 */
    BOARD_PIN_SPI0_MOSI,       /* PA7  - SPI0 数据，接 WS2812 */
    /* ---- 485 方向控制 (拉高发送/拉低接收, 默认拉低) ---- */
    BOARD_PIN_RS485_WEATHER_DIR,   /* PA8  - 气象板 */
    BOARD_PIN_RS485_AC_DIR,        /* PA15 - 空调 */
    BOARD_PIN_RS485_BRUSHLESS_DIR, /* PB3  - 无刷电机 */
    /* ---- 按键 (读低触发) ---- */
    BOARD_PIN_KEY_PLATFORM,        /* PC1 - 平台驱动 */
    BOARD_PIN_KEY_DOOR,            /* PC2 - 门驱动 */
    BOARD_PIN_KEY_EMERGENCY_STOP,  /* PC0 - 急停 */
    /* ---- 平台推杆电机 PWM ---- */
    BOARD_PIN_PLATFORM_EXTEND_PWM,  /* PA0 - 电机负 T1_CH0。 */
    BOARD_PIN_PLATFORM_RETRACT_PWM, /* PA1 - 电机正 T1_CH1。 */
    /* ---- 平台到位/限位信号 (读低触发) ---- */
    // BOARD_PIN_PLATFORM_EXTEND_SIGNAL,    /* PA4  - 推出到位信号 */
    // BOARD_PIN_PLATFORM_RETRACT_SIGNAL,   /* PA5  - 收回到位信号 */
    BOARD_PIN_PLATFORM_EXTEND_LIMIT,     /* PB12 - 推出到位限位 */
    BOARD_PIN_PLATFORM_RETRACT_LIMIT,    /* PB13 - 收回到位限位 */
    /* ---- 门电机1 PWM ---- */
    BOARD_PIN_DOOR1_EXTEND_PWM,   /* PB6 - 推出 T3_CH0 默认99% */
    BOARD_PIN_DOOR1_RETRACT_PWM,  /* PB7 - 收回 T3_CH1 默认99% */
    /* ---- 门电机2 PWM ---- */
    BOARD_PIN_DOOR2_EXTEND_PWM,   /* PB8 - 推出 T3_CH2 默认99% */
    BOARD_PIN_DOOR2_RETRACT_PWM,  /* PB9 - 收回 T3_CH3 默认99% */
    /* ---- 门电机到位信号 (读低触发) ---- */
    // BOARD_PIN_DOOR1_EXTEND_SIGNAL,   /* PC8 - 推出到位 */
    // BOARD_PIN_DOOR1_RETRACT_SIGNAL,  /* PC9 - 收回到位 */
    // BOARD_PIN_DOOR2_EXTEND_SIGNAL,   /* PC6 - 推出到位 */
    // BOARD_PIN_DOOR2_RETRACT_SIGNAL,  /* PC7 - 收回到位 */
    /* ---- 门限位信号 (读低触发) ---- */
    BOARD_PIN_DOOR_OPEN_LIMIT,       /* PB15 - 开到位限位 */
    BOARD_PIN_DOOR_CLOSE_LIMIT,      /* PB14 - 关到位限位 */
    /* ---- 其它输出 (拉低触发, 默认拉高) ---- */
    BOARD_PIN_DOOR_LOCK,         /* PB0 - 门电吸锁 */
    BOARD_PIN_LED_INDICATOR,     /* PC3 - 指示灯 */
    BOARD_PIN_LED_INTERIOR,      /* PC4 - 内部照明 */
    BOARD_PIN_DOOR_HEATER_ENABLE,  /* PC5 - 门加热解冻伴热带使能 */
    BOARD_PIN_COUNT,
    BOARD_PIN_NONE = -1,
} board_pin_id_e;

typedef enum {
    BOARD_USART_WEATHER = 0, /* USART0 - 气象板485 */
    BOARD_USART_MAINBOARD,   /* USART1 - 主板通信 */
    BOARD_USART_CHARGER,     /* USART2 - 充电板通信 */
    BOARD_USART_AC,          /* USART3 - 空调485 */
    BOARD_USART_BRUSHLESS,   /* UART4  - 无刷电机485 */
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
    BOARD_DMA_1,
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

/* 解析 dma 引用 -> 通道配置（dma-ch id-io id 链） */
#define BOARD_DMA_CH(ref) (&board_dma_configs[(ref).dma].ch[(ref).ch])
#define BOARD_DMA_HAS(ref) ((ref).dma < BOARD_DMA_COUNT)

/* 便于上层按 ID 直接读写引脚电平（含 gpio_bit_read/gpio_bit_write 的参数展开） */
#define BOARD_PIN_PORT(id)   (board_gpio_configs[id].port)
#define BOARD_PIN_PIN(id)    (board_gpio_configs[id].pin)

/* 四个光电输入的有效电平快照；冲突及到位由控制层解释。 */
typedef struct {
    bool platform_extended;
    bool platform_retracted;
    bool door_closed;
    bool door_opened;
} board_motor_limits_t;

void board_motor_io_init(void);
void board_motor_read_limits(board_motor_limits_t *limits);
void board_door_lock_write(bool locked);

#endif /* BOARD_H */
