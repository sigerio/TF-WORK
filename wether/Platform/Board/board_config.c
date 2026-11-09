#include "board_config.h"
#include "uart_transport.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_usart.h"

#define USART1_DATA_ADDRESS (USART1 + 0x04U)

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
    /* ---- 485 方向 (高=发送, 默认低=接收) ---- */
    [BOARD_PIN_RS485_MAIN_DIR] = { GPIOA, GPIO_PIN_1,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },     /* PA1  - 主板 */
    [BOARD_PIN_RS485_WIND_DIR] = { GPIOB, GPIO_PIN_2,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },     /* PB2  - 风速 */
    [BOARD_PIN_RS485_RAIN_DIR] = { GPIOB, GPIO_PIN_13,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },     /* PB13 - 原雨量逻辑路由，闲置 */
    [BOARD_PIN_RS485_TC_DIR] = { GPIOA, GPIO_PIN_8,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },      /* PA8  - 温湿度 */
    [BOARD_PIN_RS485_WATER_DIR] = { GPIOA, GPIO_PIN_15,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U },
    /* ---- 其它输出 (拉低触发, 默认拉高=关) ---- */
    
    [BOARD_PIN_LED] = { GPIOC, GPIO_PIN_7, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 1U }, /* PC7 - 指示灯 */


    [BOARD_PIN_BEEP] = { GPIOC, GPIO_PIN_1, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, 0U }, /* PC1 - 蜂鸣器  拉高触发*/
};

const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    [BOARD_USART_TC] = {
        USART0, RCU_USART0, BOARD_PIN_USART0_TX, BOARD_PIN_USART0_RX,
        BOARD_DMA_REF_NONE,
        BOARD_DMA_REF_NONE,
        BOARD_PIN_RS485_TC_DIR, 9600U,
    },
    [BOARD_USART_MAIN] = {
        USART1, RCU_USART1, BOARD_PIN_USART1_TX, BOARD_PIN_USART1_RX,
        { BOARD_DMA_0, BOARD_DMA_CH5 },          
        { BOARD_DMA_0, BOARD_DMA_CH6 },          
        BOARD_PIN_RS485_MAIN_DIR, 9600U,
    },
    [BOARD_USART_WIND] = {
        USART2, RCU_USART2, BOARD_PIN_USART2_TX, BOARD_PIN_USART2_RX,
        BOARD_DMA_REF_NONE,                       
        BOARD_DMA_REF_NONE,   
        BOARD_PIN_RS485_WIND_DIR, 4800U, /* 三合一设备出厂配置 */
    },
    [BOARD_USART_RAIN] = {
        UART4, RCU_UART4, BOARD_PIN_UART4_TX, BOARD_PIN_UART4_RX,
        BOARD_DMA_REF_NONE,        
        BOARD_DMA_REF_NONE,       
        BOARD_PIN_RS485_RAIN_DIR, 9600U,
    },
    [BOARD_USART_WATER] = {
        UART3, RCU_UART3, BOARD_PIN_USART3_TX, BOARD_PIN_USART3_RX,
        BOARD_DMA_REF_NONE,
        BOARD_DMA_REF_NONE,
        BOARD_PIN_RS485_WATER_DIR, 9600U,
    },
};

/* DMA 控制器：ch id -> (通道实例, 外设地址, 方向, io id) 链
 * 未用通道 peripheral_address=0 */
const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT] = {
    [BOARD_DMA_0] = {
        DMA0, RCU_DMA0,
        .ch = {
            [BOARD_DMA_CH5] = { DMA_CH5, USART1_DATA_ADDRESS, DMA_PERIPHERAL_TO_MEMORY, BOARD_PIN_USART1_RX },
            [BOARD_DMA_CH6] = { DMA_CH6, USART1_DATA_ADDRESS, DMA_MEMORY_TO_PERIPHERAL, BOARD_PIN_USART1_TX },
        },
    },

};

void board_gpio_init(board_pin_id_e pin_id)
{
    const board_gpio_config_t *config = &board_gpio_configs[pin_id];

    gpio_bit_write(config->port, config->pin,
                   config->initial_level ? SET : RESET);
    gpio_init(config->port, config->mode, config->speed, config->pin);
}

void board_init(void)
{
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    board_gpio_init(BOARD_PIN_RS485_MAIN_DIR);
    board_gpio_init(BOARD_PIN_RS485_TC_DIR);
    board_gpio_init(BOARD_PIN_RS485_WIND_DIR);
    board_gpio_init(BOARD_PIN_RS485_RAIN_DIR);
    board_gpio_init(BOARD_PIN_RS485_WATER_DIR);
    board_gpio_init(BOARD_PIN_LED);
    board_gpio_init(BOARD_PIN_BEEP);

    uart_transport_init();
}
