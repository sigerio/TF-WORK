#include "BOARD/board.h"

#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_spi.h"
#include "gd32f10x_usart.h"

/* 外设数据寄存器地址 (用于 DMA 外设地址)
 * 直接用「基址 + 偏移」的常量表达式，避免对寄存器宏取址触发 #1296-D */
#define USART0_DATA_ADDRESS (USART0 + 0x04U)
#define USART1_DATA_ADDRESS (USART1 + 0x04U)
#define USART2_DATA_ADDRESS (USART2 + 0x04U)
#define USART3_DATA_ADDRESS (UART3 + 0x04U)
#define UART4_DATA_ADDRESS  (UART4 + 0x04U)
#define SPI0_DATA_ADDRESS   (SPI0 + 0x0CU)

/*
 * GPIO 模式约定:
 *   读低触发输入       -> 上拉输入 IPU
 *   拉低触发输出(灯/锁) -> 推挽输出, 默认拉高(关)
 *   485 方向(高发送)    -> 推挽输出, 默认拉低(接收)
 *   PWM 高电平触发输出   -> 复用推挽, 默认拉高(默认99%高)
 */
const board_gpio_config_t board_gpio_configs[BOARD_PIN_COUNT] = {
    /* ---- 串口 ---- */
    [BOARD_PIN_USART0_TX] = { GPIOA, GPIO_PIN_9,  GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART0_RX] = { GPIOA, GPIO_PIN_10, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART1_TX] = { GPIOA, GPIO_PIN_2,  GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART1_RX] = { GPIOA, GPIO_PIN_3,  GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART2_TX] = { GPIOB, GPIO_PIN_10, GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART2_RX] = { GPIOB, GPIO_PIN_11, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART3_TX] = { GPIOC, GPIO_PIN_10, GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_USART3_RX] = { GPIOC, GPIO_PIN_11, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_UART4_TX]  = { GPIOC, GPIO_PIN_12, GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_UART4_RX]  = { GPIOD, GPIO_PIN_2,  GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 灯带 SPI0 默认映射：PA5=SCK、PA7=MOSI ---- */
    [BOARD_PIN_SPI0_SCK]  = { GPIOA, GPIO_PIN_5,  GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_SPI0_MOSI] = { GPIOA, GPIO_PIN_7,  GPIO_MODE_AF_PP,       GPIO_OSPEED_50MHZ, 0U },
    /* ---- 485 方向 (高=发送, 默认低=接收) ---- */
    [BOARD_PIN_RS485_WEATHER_DIR]   = { GPIOA, GPIO_PIN_8,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_RS485_AC_DIR]        = { GPIOA, GPIO_PIN_15, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_RS485_BRUSHLESS_DIR] = { GPIOB, GPIO_PIN_3,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 按键 (读低触发) ---- */
    [BOARD_PIN_KEY_PLATFORM]       = { GPIOC, GPIO_PIN_1, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_KEY_DOOR]           = { GPIOC, GPIO_PIN_2, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_KEY_EMERGENCY_STOP] = { GPIOC, GPIO_PIN_0, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 平台电机 PWM：PA0=伸出，PA1=收回 ---- */
    [BOARD_PIN_PLATFORM_EXTEND_PWM]  = { GPIOA, GPIO_PIN_0, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_PLATFORM_RETRACT_PWM] = { GPIOA, GPIO_PIN_1, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    /* ---- 平台到位/限位 (读低触发) ---- */
    // [BOARD_PIN_PLATFORM_EXTEND_SIGNAL]  = { GPIOA, GPIO_PIN_4,  GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    // [BOARD_PIN_PLATFORM_RETRACT_SIGNAL] = { GPIOA, GPIO_PIN_5,  GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_PLATFORM_EXTEND_LIMIT]   = { GPIOB, GPIO_PIN_12, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_PLATFORM_RETRACT_LIMIT]  = { GPIOB, GPIO_PIN_13, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 门电机1 PWM ---- */
    [BOARD_PIN_DOOR1_EXTEND_PWM]  = { GPIOB, GPIO_PIN_6, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_DOOR1_RETRACT_PWM] = { GPIOB, GPIO_PIN_7, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    /* ---- 门电机2 PWM ---- */
    [BOARD_PIN_DOOR2_EXTEND_PWM]  = { GPIOB, GPIO_PIN_8, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_DOOR2_RETRACT_PWM] = { GPIOB, GPIO_PIN_9, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, 1U },
    /* ---- 门电机到位信号 (读低触发) ---- */
    // [BOARD_PIN_DOOR1_EXTEND_SIGNAL]  = { GPIOC, GPIO_PIN_8, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    // [BOARD_PIN_DOOR1_RETRACT_SIGNAL] = { GPIOC, GPIO_PIN_9, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    // [BOARD_PIN_DOOR2_EXTEND_SIGNAL]  = { GPIOC, GPIO_PIN_6, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    // [BOARD_PIN_DOOR2_RETRACT_SIGNAL] = { GPIOC, GPIO_PIN_7, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 门限位信号 (读低触发) ---- */
    [BOARD_PIN_DOOR_OPEN_LIMIT]  = { GPIOB, GPIO_PIN_15, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    [BOARD_PIN_DOOR_CLOSE_LIMIT] = { GPIOB, GPIO_PIN_14, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 其它输出 (拉低触发, 默认拉高=关) ---- */
    [BOARD_PIN_DOOR_LOCK]        = { GPIOB, GPIO_PIN_0, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_LED_INDICATOR]    = { GPIOC, GPIO_PIN_3, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_LED_INTERIOR]     = { GPIOC, GPIO_PIN_4, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 1U },
    [BOARD_PIN_DOOR_HEATER_ENABLE] = { GPIOC, GPIO_PIN_5, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 1U },
};

const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    [BOARD_USART_WEATHER] = {
        USART0, RCU_USART0, BOARD_PIN_USART0_TX, BOARD_PIN_USART0_RX,
        { BOARD_DMA_0, BOARD_DMA_CH4 },          /* RX: DMA0_CH4 */
        { BOARD_DMA_0, BOARD_DMA_CH3 },          /* TX: DMA0_CH3 */
        BOARD_PIN_RS485_WEATHER_DIR, 9600U,
    },
    [BOARD_USART_MAINBOARD] = {
        USART1, RCU_USART1, BOARD_PIN_USART1_TX, BOARD_PIN_USART1_RX,
        { BOARD_DMA_0, BOARD_DMA_CH5 },          /* RX: DMA0_CH5 */
        { BOARD_DMA_0, BOARD_DMA_CH6 },          /* TX: DMA0_CH6 */
        BOARD_PIN_NONE, 115200U,
    },
    [BOARD_USART_CHARGER] = {
        USART2, RCU_USART2, BOARD_PIN_USART2_TX, BOARD_PIN_USART2_RX,
        BOARD_DMA_REF_NONE,                       /* RX: DMA0_CH2 被 SPI0_TX 占用, 走 RXNE 中断 */
        { BOARD_DMA_0, BOARD_DMA_CH1 },          /* TX: DMA0_CH1 */
        BOARD_PIN_NONE, 1000000,
    },
    [BOARD_USART_AC] = {
        UART3, RCU_UART3, BOARD_PIN_USART3_TX, BOARD_PIN_USART3_RX,
        { BOARD_DMA_1, BOARD_DMA_CH2 },          /* RX: DMA1_CH2 */
        { BOARD_DMA_1, BOARD_DMA_CH4 },          /* TX: DMA1_CH4 */
        BOARD_PIN_RS485_AC_DIR, 9600U,
    },
    [BOARD_USART_BRUSHLESS] = {
        UART4, RCU_UART4, BOARD_PIN_UART4_TX, BOARD_PIN_UART4_RX,
        BOARD_DMA_REF_NONE,                       /* RX: 无 DMA（预留） */
        BOARD_DMA_REF_NONE,                       /* TX: 无 DMA（预留） */
        BOARD_PIN_RS485_BRUSHLESS_DIR, 9600U,
    },
};

/* DMA 控制器：ch id -> (通道实例, 外设地址, 方向, io id) 链
 * 未用通道 peripheral_address=0 */
const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT] = {
    [BOARD_DMA_0] = {
        DMA0, RCU_DMA0,
        .ch = {
            [BOARD_DMA_CH1] = { DMA_CH1, USART2_DATA_ADDRESS, DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_USART2_TX },
            [BOARD_DMA_CH2] = { DMA_CH2, SPI0_DATA_ADDRESS,   DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_SPI0_MOSI },
            [BOARD_DMA_CH3] = { DMA_CH3, USART0_DATA_ADDRESS, DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_USART0_TX },
            [BOARD_DMA_CH4] = { DMA_CH4, USART0_DATA_ADDRESS, DMA_PERIPHERAL_TO_MEMORY, BOARD_PIN_USART0_RX },
            [BOARD_DMA_CH5] = { DMA_CH5, USART1_DATA_ADDRESS, DMA_PERIPHERAL_TO_MEMORY, BOARD_PIN_USART1_RX },
            [BOARD_DMA_CH6] = { DMA_CH6, USART1_DATA_ADDRESS, DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_USART1_TX },
        },
    },
    [BOARD_DMA_1] = {
        DMA1, RCU_DMA1,
        .ch = {
            [BOARD_DMA_CH2] = { DMA_CH2, USART3_DATA_ADDRESS, DMA_PERIPHERAL_TO_MEMORY, BOARD_PIN_USART3_RX },
            [BOARD_DMA_CH4] = { DMA_CH4, USART3_DATA_ADDRESS, DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_USART3_TX },
        },
    },
};

const board_spi_config_t board_spi_configs[BOARD_SPI_COUNT] = {
    /* 108MHz(APB2) / 16 = 6.75MHz，8 个 SPI 位编码 1 个 WS2812 位，约 844kHz */
    [BOARD_SPI_WS2812] = {
        .peripheral = SPI0,
        .clock = RCU_SPI0,
        .sck_pin = BOARD_PIN_SPI0_SCK,
        .mosi_pin = BOARD_PIN_SPI0_MOSI,
        .tx_dma = { BOARD_DMA_0, BOARD_DMA_CH2 },
        .prescale = SPI_PSC_16,
    },
};

/* 108MHz/(107+1)=1MHz 计数, period=999 -> 1kHz PWM, 默认99%高电平
 * ch id -> (通道实例, io id) 链 */
const board_pwm_config_t board_pwm_configs[BOARD_PWM_COUNT] = {
    [BOARD_PWM_PLATFORM] = {
        TIMER1, RCU_TIMER1, 107U, 999U,
        .ch = {
            [BOARD_PWM_CH0] = { TIMER_CH_0, BOARD_PIN_PLATFORM_EXTEND_PWM },
            [BOARD_PWM_CH1] = { TIMER_CH_1, BOARD_PIN_PLATFORM_RETRACT_PWM },
        },
    },
    [BOARD_PWM_DOOR] = {
        TIMER3, RCU_TIMER3, 107U, 999U,
        .ch = {
            [BOARD_PWM_CH0] = { TIMER_CH_0, BOARD_PIN_DOOR1_EXTEND_PWM },
            [BOARD_PWM_CH1] = { TIMER_CH_1, BOARD_PIN_DOOR1_RETRACT_PWM },
            [BOARD_PWM_CH2] = { TIMER_CH_2, BOARD_PIN_DOOR2_EXTEND_PWM },
            [BOARD_PWM_CH3] = { TIMER_CH_3, BOARD_PIN_DOOR2_RETRACT_PWM },
        },
    },
};

void board_motor_io_init(void)
{
    static const board_pin_id_e inputs[] = {
        BOARD_PIN_PLATFORM_EXTEND_LIMIT, BOARD_PIN_PLATFORM_RETRACT_LIMIT,
        BOARD_PIN_DOOR_CLOSE_LIMIT, BOARD_PIN_DOOR_OPEN_LIMIT
    };

    rcu_periph_clock_enable(RCU_GPIOB);
    for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        const board_gpio_config_t *pin = &board_gpio_configs[inputs[i]];
        gpio_init(pin->port, pin->mode, pin->speed, pin->pin);
    }
    board_door_lock_write(false);
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK), board_gpio_configs[BOARD_PIN_DOOR_LOCK].mode,
        board_gpio_configs[BOARD_PIN_DOOR_LOCK].speed, BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK));
}

void board_motor_read_limits(board_motor_limits_t *limits)
{
    if (limits == 0) return;
    limits->platform_extended = gpio_input_bit_get(BOARD_PIN_PORT(BOARD_PIN_PLATFORM_EXTEND_LIMIT), BOARD_PIN_PIN(BOARD_PIN_PLATFORM_EXTEND_LIMIT)) == RESET;
    limits->platform_retracted = gpio_input_bit_get(BOARD_PIN_PORT(BOARD_PIN_PLATFORM_RETRACT_LIMIT), BOARD_PIN_PIN(BOARD_PIN_PLATFORM_RETRACT_LIMIT)) == RESET;
    limits->door_closed = gpio_input_bit_get(BOARD_PIN_PORT(BOARD_PIN_DOOR_CLOSE_LIMIT), BOARD_PIN_PIN(BOARD_PIN_DOOR_CLOSE_LIMIT)) == RESET;
    limits->door_opened = gpio_input_bit_get(BOARD_PIN_PORT(BOARD_PIN_DOOR_OPEN_LIMIT), BOARD_PIN_PIN(BOARD_PIN_DOOR_OPEN_LIMIT)) == RESET;
}

void board_door_lock_write(bool locked)
{
    gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK), BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK), locked ? RESET : SET);
}
