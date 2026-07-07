/**
 * @file    tf_drv_uart.h
 * @brief   UART 驱动接口
 *
 * 用户配置流程：
 *   1. 定义 tf_drv_uart_id_e 枚举（含 E_DRV_UART_NONE = -1 哨兵）
 *   2. 填写 tf_drv_uart_config_t 配置数组（命名固定为 uart_configs）
 *   3. 确保枚举值 < E_DRV_UART_MAX
 *   4. 调 tf_drv_uart_register() → 返回句柄
 *   5. 通过句柄调 tx/rx 收发数据
 *
 * @code
 *   // 枚举（用户定义）
 *   typedef enum {
 *       E_DRV_UART_NONE = -1,
 *       E_DRV_UART_0,
 *       E_DRV_UART_MAX
 *   } tf_drv_uart_id_e;
 *
 *   // 配置表
 *   tf_drv_uart_config_t uart_configs[E_DRV_UART_MAX] = {
 *       [E_DRV_UART_0] = {
 *           .base      = (void *)USART0,
 *           .baudrate  = 115200U,
 *           .io        = { .tx_id = E_IO_UART0_TX, .rx_id = E_IO_UART0_RX },
 *       },
 *   };
 *
 *   // 初始化 + 收发
 *   tf_drv_uart_handle_t h = tf_drv_uart_register(E_DRV_UART_0);
 *   tf_drv_uart_tx(h, (const uint8_t *)"hello", 5);
 * @endcode
 */
#ifndef TF_DRV_UART_H
#define TF_DRV_UART_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 

#ifdef __cplusplus
extern "C" {
#endif


/* ========== 类型定义 ========== */

/**
 * @brief UART 引脚描述（引用 IO 子系统中的引脚 ID）
 */
typedef struct {
    int tx_id;
    int rx_id;
} tf_drv_uart_io_cfg_t;

/**
 * @brief UART 配置结构体——只保留资源描述字段
 *
 * `baudrate` 作为板级静态配置项保留在 config 中；
 * 其他初始化参数（word_length/stop_bits/parity/mode）仍由 HAL 层提供。
 */
typedef struct tf_drv_uart_config {
    void                 *base;      /**< UART 外设基地址                   */
    uint32_t              baudrate;  /**< UART 波特率（板级静态配置）       */
    tf_drv_uart_io_cfg_t  io;        /**< IO 依赖：TX/RX 引脚 ID             */
} tf_drv_uart_config_t;

#if MCU_CONTAIN_PRE_USART
extern tf_drv_uart_config_t uart_configs[];
#endif

/** @brief UART 设备句柄（不透明指针） */
typedef void * tf_drv_uart_handle_t;

/* ========== 初始化 ========== */

/**
 * @brief 注册 UART 设备
 *
 * 从 uart_configs[] 读取配置，自动完成 GPIO 引脚初始化，
 * 配置 UART 外设寄存器并使能。
 *
 * @param id  UART 枚举 ID（须在 [0, E_DRV_UART_MAX) 范围内）
 * @return    句柄，失败返回 NULL
 */
tf_drv_uart_handle_t tf_drv_uart_register(int id);

/**
 * @brief 根据枚举 ID 获取已注册的设备句柄
 *
 * @param id  UART 枚举 ID
 * @return    句柄（未注册返回 NULL）
 */
tf_drv_uart_handle_t tf_drv_uart_get_handle(int id);

/**
 * @brief 检查 UART 设备是否已注册
 *
 * @param id  UART 枚举 ID
 * @return    true=已注册 / false=未注册
 */
bool tf_drv_uart_is_registered(int id);

/* ========== 数据收发 ========== */

/**
 * @brief 发送数据
 *
 * @param h     设备句柄
 * @param data  数据缓冲区
 * @param len   发送长度
 */
void tf_drv_uart_tx(tf_drv_uart_handle_t h, const uint8_t *data, uint32_t len);

/**
 * @brief 接收数据
 *
 * @param h    设备句柄
 * @param buf  接收缓冲区
 * @param len  期望接收长度
 */
void tf_drv_uart_rx(tf_drv_uart_handle_t h, uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_UART_H */
