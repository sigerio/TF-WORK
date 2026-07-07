/**
 * @file    tf_drv_i2c.h
 * @brief   I2C 总线外设配置与通信控制
 *
 * I2C（Inter-Integrated Circuit）同步串行总线外设，支持标准模式
 * （100kHz）和快速模式（400kHz），7-bit 和 10-bit 地址，主/从模式。
 *
 * GD32F10x 处理器集成 2 个 I2C 控制器（I2C0 / I2C1）。
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_i2c_id_e 枚举（含 E_I2C_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_i2c_config_t 配置数组（命名 i2c_configs）
 * 3. 调用 tf_drv_i2c_register() 完成 I2C 控制器初始化
 * 4. 主模式收发：master_tx() / master_rx() / master_tx_rx()
 *
 * 示例——主模式发送（GD32：Master_transmitter）：
 * @code
 *   typedef enum { E_I2C_NONE=-1, E_I2C_0, E_I2C_MAX } tf_drv_i2c_id_e;
 *
 *   i2c_configs[E_I2C_0] = {
 *       .base        = (void *)I2C0,
 *       .io          = { .scl_id = E_IO_I2C0_SCL, .sda_id = E_IO_I2C0_SDA },
 *   };
 *
 *   tf_drv_i2c_handle_t h = tf_drv_i2c_register(E_I2C_0);
 *
 *   // 高级 API：向 0x50 发 16 字节
 *   uint8_t tx[] = {0xAB, 0xCD, 0xEF};
 *   tf_drv_i2c_master_tx(h, 0x50, tx, 3);
 *
 * @endcode
 *
 * ============ 架构说明 ============
 *
 * I2C 初始化参数（timing/clock_speed/addformat/own_addr）已移入 HAL 层，
 * 各平台 HAL 在 tf_hal_i2c_init() 内以 per-instance 回调或硬编码提供。
 * DRV config 只保留资源描述字段（base + IO ID）。
 *
 * ============ 配置宏 ============
 */

#ifndef TF_DRV_I2C_H
#define TF_DRV_I2C_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 



#ifdef __cplusplus
extern "C" {
#endif


/* ========== 类型定义 ========== */

/**
 * @brief I2C 引脚配置子结构体
 */
typedef struct {
    int scl_id;
    int sda_id;
} tf_drv_i2c_io_cfg_t;

/**
 * @brief I2C 配置结构体——只保留资源描述字段
 *
 * 初始化参数（timing/clock_speed/addformat/own_addr）已移入 HAL 层，
 * 各平台 HAL 在 tf_hal_i2c_init() 内以 per-instance 回调或硬编码提供。
 */
typedef struct {
    void                *base;        /**< I2C 实例（如 I2C1 / I2C2）         */
    tf_drv_i2c_io_cfg_t  io;          /**< IO 依赖：SCL/SDA 引脚 ID           */
} tf_drv_i2c_config_t;

#if MCU_CONTAIN_PRE_I2C
extern tf_drv_i2c_config_t i2c_configs[];
#endif

/**
 * @brief I2C 设备句柄（不透明指针）
 */
typedef void * tf_drv_i2c_handle_t;
int tf_drv_i2c_master_tx(tf_drv_i2c_handle_t h, uint8_t addr, const uint8_t *data, uint32_t len);
int tf_drv_i2c_master_rx(tf_drv_i2c_handle_t h, uint8_t addr, uint8_t *buf, uint32_t len);
int tf_drv_i2c_master_tx_rx(tf_drv_i2c_handle_t h, uint8_t addr, const uint8_t *tx_data, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);

/* ========== 对外接口 ========== */

/**
 * @brief 注册 I2C 设备
 *
 * 完成 I2C 控制器初始化：复位寄存器 → 配置时钟 → 配置地址 → 使能 → 使能 ACK。
 */
tf_drv_i2c_handle_t tf_drv_i2c_register(int id);

/**
 * @brief 获取句柄
 */
tf_drv_i2c_handle_t tf_drv_i2c_get_handle(int id);

/**
 * @brief 检查是否已注册
 */
bool tf_drv_i2c_is_registered(int id);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_I2C_H */
