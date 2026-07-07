#include "tf_board_uart.h"

#if MCU_CONTAIN_PRE_USART
tf_drv_uart_config_t uart_configs[E_DRV_UART_MAX] = {
    [E_DRV_UART_0] = {
        .base        = (void *)USART0,
        .baudrate    = 115200U,
        .io          = { .tx_id = E_IO_U0_TX, .rx_id = E_IO_U0_RX },
    },
    [E_DRV_UART_1] = {
        .base        = (void *)USART1,
        .baudrate    = 115200U,
        .io          = { .tx_id = E_IO_U1_TX, .rx_id = E_IO_U1_RX },
    },
    [E_DRV_UART_2] = {
        .base        = (void *)USART2, /* GD32 USART2，对应常见 USART3 */
        .baudrate    = 9600U,
        .io          = { .tx_id = E_IO_U2_TX, .rx_id = E_IO_U2_RX },
    },
};
#endif /* MCU_CONTAIN_PRE_USART */
