#ifndef __BOARD_UART_H__
#define __BOARD_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 板级配置（当前资源枚举） */
#include "tf_board_config.h"
/* IO 依赖（引脚枚举）*/
#include "tf_board_io.h"
/* dev 层接口 */
#include "tf_drv_uart.h"

extern tf_drv_uart_config_t uart_configs[E_DRV_UART_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_UART_H__ */
