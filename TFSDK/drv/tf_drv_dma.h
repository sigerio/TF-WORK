/**
 * @file    tf_drv_dma.h
 * @brief   DMA 外设注册与通道传输控制
 *
 * DMA 在 TF SDK 中建模为"父设备 + 子通道"：
 * - DMA 控制器（DMA0、DMA1...）是父设备
 * - 通道（CH0、CH1...）是内部子结构
 * - 通道静态配置放在 dma_configs[].ch[]
 * - `tf_drv_dma_register()` 内部完成父设备初始化与所有通道初始化
 * - 通道不含 IO 属性，但可携带 DMA 地址类静态配置
 * - DMA ↔ 外设（UART/SPI/I2C/ADC 等）的绑定由用户在平台层完成
 * - 通道方向由 board_dma.c 显式给出，避免 USART/SPI 混合映射时仅靠通道号推断
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_dma_id_e 枚举（含 E_DRV_DMA_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_dma_config_t 配置数组（命名 dma_configs）
 * 3. 用户确保枚举值 < E_DRV_DMA_MAX
 * 4. 调用 tf_drv_dma_register() 完成 DMA 控制器及所有通道初始化
 * 5. 通过句柄 + 通道号调用 start/stop/addr_config/number_config
 *
 * 示例：
 * @code
 *   typedef enum {
 *       E_DRV_DMA_NONE = -1,
 *       E_DRV_DMA_0,                  // DMA0
 *       E_DRV_DMA_MAX
 *   } tf_drv_dma_id_e;
 *
 *   dma_configs[E_DRV_DMA_0] = {
 *       .base          = (void *)DMA0,
 *       .channel_count = 1,
 *       .ch = {
 *           [0] = {  // CH0: USART1_TX
 *               .channel     = DMA_CH3,
 *               .periph_addr = (uint32_t)&USART_DATA(USART0),
 *               .memory_addr = 0U,
 *               .enabled     = true,
 *           },
 *       },
 *   };
 *
 *   tf_drv_dma_handle_t h = tf_drv_dma_register(E_DRV_DMA_0);
 *   // 运行时设置地址和长度
 *   tf_drv_dma_addr_config(h, 0, (uint32_t)&USART_DATA(USART0), (uint32_t)tx_buf);
 *   tf_drv_dma_number_config(h, 0, len);
 *   tf_drv_dma_start(h, 0);
 * @endcode
 *
 */

#ifndef TF_DRV_DMA_H
#define TF_DRV_DMA_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 


#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

/**
 * @brief DMA 通道配置——只保留资源描述
 *
 * direction 由 Board Port 显式配置；width/priority/request 等初始化参数
 * 由平台 HAL 提供默认值或后续按需扩展。
 */
typedef struct tf_drv_dma_ch_config {
    uint32_t channel;      /**< 平台原生通道号/请求号                           */
    uint32_t periph_addr;  /**< 外设地址，类型与官方库 dma_parameter_struct 一致 */
    uint32_t memory_addr;  /**< 内存地址，类型与官方库 dma_parameter_struct 一致 */
    uint8_t  direction;    /**< DMA_MEMORY_TO_PERIPHERAL / DMA_PERIPHERAL_TO_MEMORY */
    bool     enabled;      /**< 通道是否启用                                   */
} tf_drv_dma_ch_cfg_t;

/**
 * @brief DMA 父设备配置结构体——只保留资源描述
 *
 * 通道初始化参数由 CubeMX 或 Board Port 提供。
 */
typedef struct tf_drv_dma_config {
    void                *base;           /**< DMA 控制器基地址（如 DMA0 / DMA1） */
    uint8_t              channel_count;  /**< 有效通道数；0 表示使用最大容量     */
    tf_drv_dma_ch_cfg_t  ch[E_DRV_DX_MAX];  /**< 通道配置数组              */
} tf_drv_dma_config_t;

#if MCU_CONTAIN_PRE_DMA
extern tf_drv_dma_config_t dma_configs[];
#endif

/**
 * @brief DMA 设备句柄（不透明指针）
 *
 * 上层通过此句柄操作 DMA，不感知 DMA 基地址等细节。
 * 通道操作需额外传入通道索引。
 */
typedef void * tf_drv_dma_handle_t;

/* ========== 对外接口 ========== */

/**
 * @brief 注册 DMA 控制器
 *
 * 根据枚举 ID 从用户提供的 dma_configs[] 中读取配置，
 * 初始化 DMA 控制器硬件，遍历所有通道调用 HAL 初始化。
 *
 * @param id  DMA 枚举 ID（须在 [0, E_DRV_DMA_MAX) 范围内）
 * @return    成功返回句柄，失败返回 NULL
 */
tf_drv_dma_handle_t tf_drv_dma_register(int id);

/**
 * @brief 根据枚举 ID 获取已注册的设备句柄
 *
 * @param id  DMA 枚举 ID
 * @return    句柄（未注册或无效 ID 返回 NULL）
 */
tf_drv_dma_handle_t tf_drv_dma_get_handle(int id);

/**
 * @brief 检查 DMA 设备是否已注册
 *
 * @param id  DMA 枚举 ID
 * @return    true=已注册 / false=未注册
 */
bool tf_drv_dma_is_registered(int id);

/* ========== 通道运行时接口 ========== */

/**
 * @brief 启动指定 DMA 通道传输
 *
 * @param h         设备句柄
 * @param ch_index  通道索引（0~E_DRV_DX_MAX-1）
 */
void tf_drv_dma_start(tf_drv_dma_handle_t h, uint8_t ch_index);

/**
 * @brief 停止指定 DMA 通道传输
 *
 * @param h         设备句柄
 * @param ch_index  通道索引
 */
void tf_drv_dma_stop(tf_drv_dma_handle_t h, uint8_t ch_index);

/**
 * @brief 配置指定通道的源/目标地址
 *
 * @param h            设备句柄
 * @param ch_index     通道索引
 * @param periph_addr  外设地址
 * @param memory_addr  内存地址
 */
void tf_drv_dma_addr_config(tf_drv_dma_handle_t h, uint8_t ch_index,
                             uint32_t periph_addr, uint32_t memory_addr);

/**
 * @brief 配置指定通道的传输数据量
 *
 * @param h         设备句柄
 * @param ch_index  通道索引
 * @param number    传输数量
 */
void tf_drv_dma_number_config(tf_drv_dma_handle_t h, uint8_t ch_index,
                               uint32_t number);

/**
 * @brief 获取指定通道剩余传输量
 *
 * @param h         设备句柄
 * @param ch_index  通道索引
 * @return          剩余传输量
 */
uint32_t tf_drv_dma_remaining_get(tf_drv_dma_handle_t h, uint8_t ch_index);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_DMA_H */
