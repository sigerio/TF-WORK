/**
 * @file    tf_drv_spi.h
 * @brief   SPI 外设注册与数据收发
 *
 * 本层职责：
 *   - 管理 SPI 硬件端口资源的注册
 *   - 提供基础的数据发送 / 接收 / 全双工收发操作
 *   - 通过不透明句柄隔离上层对硬件细节的依赖
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_spi_id_e 枚举（含 E_SPI_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_spi_config_t 配置数组（命名 spi_configs）
 * 3. 用户确保枚举值小于 E_DRV_SPI_MAX
 * 4. 调用 tf_drv_spi_register() 完成初始化并获得句柄
 * 5. 通过句柄调用 tx / rx / txrx 进行数据收发
 *
 * 示例：
 * @code
 *   typedef enum {
 *       E_SPI_NONE = -1,
 *       E_SPI_0,
 *       E_SPI_1,
 *       E_SPI_MAX
 *   } tf_drv_spi_id_e;
 *
 *   tf_drv_spi_config_t spi_configs[E_SPI_MAX] = {
 *       [E_SPI_0] = { .base       = (void *)SPI0,
 *                      .io         = { .mosi_id = E_IO_SPI0_MOSI, .miso_id = E_IO_SPI0_MISO,
 *                                      .sck_id = E_IO_SPI0_SCK, .cs_id = E_IO_SPI0_CS } },
 *   };
 *
 *   tf_drv_spi_register(E_SPI_0);
 *   tf_drv_spi_tx(tf_drv_spi_get_handle(E_SPI_0),
 *                   (const uint8_t *)"hello", 5);
 * @endcode
 *
 * ============ 架构说明 ============
 *
 * SPI 初始化参数由 HAL 层 per-instance 回调提供（移植自 Core/Src/spi.c），
 * DRV config 只保留资源描述字段（base + IO ID），
 * 不重复定义 CubeMX 已生成的初始化参数。
 *
 */

#ifndef TF_DRV_SPI_H
#define TF_DRV_SPI_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 



#ifdef __cplusplus
extern "C" {
#endif


/* ========== 类型定义 ========== */

/**
 * @brief SPI 引脚描述（引用 IO 子系统中的引脚 ID）
 *
 * 引脚的真实 port/pin/mode 信息只存在于 io_configs[] 中。
 * 此处仅存储 IO 枚举 ID，register() 时委托 IO 子系统完成 GPIO 初始化。
 */
typedef struct {
    int mosi_id;
    int miso_id;
    int sck_id;
    int cs_id;
} tf_drv_spi_io_cfg_t;

/**
 * @brief SPI 配置结构体——只保留资源描述字段
 *
 * 初始化参数（mode/cpol/cpha/prescaler 等）由 HAL 层 per-instance 回调提供，
 * 移植自 Core/Src/spi.c。DRV config 不重复定义 CubeMX 已生成的参数。
 *
 * register() 时会先根据 .io.* 自动配置 GPIO 复用功能，
 * 再初始化 SPI 外设寄存器。
 */
typedef struct {
    void               *base;       /**< SPI 外设基地址                       */
    tf_drv_spi_io_cfg_t  io;        /**< IO 依赖：MOSI/MISO/SCK/CS            */
} tf_drv_spi_config_t;

#if MCU_CONTAIN_PRE_SPI
extern tf_drv_spi_config_t spi_configs[];
#endif

/**
 * @brief SPI 设备句柄（不透明指针）
 *
 * 上层通过此句柄操作 SPI，不感知 SPI 基地址、波特率等细节。
 */
typedef void * tf_drv_spi_handle_t;
void tf_drv_spi_rx(tf_drv_spi_handle_t h, uint8_t *buf, uint32_t len);
void tf_drv_spi_tx(tf_drv_spi_handle_t h, const uint8_t *data, uint32_t len);
void tf_drv_spi_txrx(tf_drv_spi_handle_t h, const uint8_t *tx_data, uint8_t *rx_buf, uint32_t len);

/* ========== 对外接口 ========== */

/**
 * @brief 注册 SPI 设备
 *
 * 根据枚举 ID 从用户提供的 spi_configs[] 中读取配置，
 * 初始化硬件 SPI 外设寄存器，返回不透明句柄。
 *
 * @param id  SPI 枚举 ID（须在 [0, E_DRV_SPI_MAX) 范围内）
 * @return    成功返回句柄，失败返回 NULL（id 超出范围）
 */
tf_drv_spi_handle_t tf_drv_spi_register(int id);

/**
 * @brief 根据枚举 ID 获取已注册的设备句柄
 *
 * @param id  SPI 枚举 ID
 * @return    句柄（未注册或无效 ID 返回 NULL）
 */
tf_drv_spi_handle_t tf_drv_spi_get_handle(int id);

/**
 * @brief 检查 SPI 设备是否已注册
 *
 * @param id  SPI 枚举 ID
 * @return    true=已注册 / false=未注册
 */
bool tf_drv_spi_is_registered(int id);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_SPI_H */
