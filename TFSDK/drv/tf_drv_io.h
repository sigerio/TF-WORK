/**
 * @file    tf_drv_io.h
 * @brief   IO 外设注册与原子操作
 *
 * 本层职责：
 *   - 管理 IO 硬件端口资源的注册
 *   - 提供基础的原子读写操作
 *   - 通过不透明句柄隔离上层对硬件细节的依赖
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_io_id_e 枚举（含 E_IO_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_io_config_t 配置数组（命名 io_configs）
 * 3. 用户确保枚举值小于 E_IO_MAX
 * 4. 调用 tf_drv_io_register() 完成初始化并获得句柄
 * 5. 通过句柄调用 set/get/toggle 操作
 *
 * 示例：
 * @code
 *   // 1. 枚举
 *   typedef enum {
 *       E_IO_NONE = -1,
 *       E_IO_0,
 *       E_IO_1,
 *       E_IO_2,
 *       E_IO_MAX
 *   } tf_drv_io_id_e;
 *
 *   // 2. 配置数组（命名固定为 io_configs）
 *   tf_drv_io_config_t io_configs[E_IO_MAX] = {
 *       [E_IO_0]   = { .port = (uintptr_t)GPIOx, .pin = X, .mode = GPIO_MODE_OUTPUT_PP,
 *                       .pull = GPIO_NOPULL, .speed = GPIO_SPEED_FREQ_LOW,
 *                       .alternate = 0, .init_state = 0 },
 *       [E_IO_1]   = { .port = (uintptr_t)GPIOy, .pin = Y, .mode = GPIO_MODE_INPUT,
 *                       .pull = GPIO_PULLUP, .speed = GPIO_SPEED_FREQ_LOW,
 *                       .alternate = 0, .init_state = 0 },
 *   };
 *
 *   // 3. 注册
 *   tf_drv_io_register(E_IO_0);
 *   // 4. 使用
 *   tf_drv_io_set(tf_drv_io_get_handle(E_IO_0), 1);
 * @endcode
 *
 *
 * 
 */

#ifndef TF_DRV_IO_H
#define TF_DRV_IO_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

/**
 * @brief IO 引脚配置结构体
 *
 * 由用户在编译期填写，通过 io_configs[] 数组提供。
 */
typedef struct {
    uintptr_t port;       /**< GPIO 端口基地址（如 (uintptr_t)GPIOA） */
    uint16_t  pin;        /**< 引脚号（0~15）                        */
    uint32_t  mode;       /**< GPIO 模式（GPIO_MODE_INPUT / _OUTPUT_PP / _AF_PP / ...） */
    uint8_t   pull;       /**< 上拉/下拉（GPIO_NOPULL / GPIO_PULLUP / GPIO_PULLDOWN） */
    uint8_t   speed;      /**< 速度（GPIO_SPEED_FREQ_LOW / _MEDIUM / _HIGH / _VERY_HIGH） */
    uint32_t  alternate;  /**< 复用功能编号（非复用模式下填 0） */
    uint8_t   init_state; /**< 引脚初始电平（0/1，register 时写入硬件 ODR） */
} tf_drv_io_config_t;

extern tf_drv_io_config_t io_configs[];

/**
 * @brief IO 设备句柄（不透明指针）
 *
 * 上层通过此句柄操作 IO，不感知引脚编号等硬件细节。
 */
typedef void * tf_drv_io_handle_t;

/* ========== 对外接口 ========== */

/**
 * @brief 注册 IO 设备
 *
 * 根据枚举 ID 从用户提供的 io_configs[] 中读取配置，
 * 初始化硬件 GPIO 寄存器，返回不透明句柄。
 *
 * @param id  IO 枚举 ID（须在 [0, E_IO_MAX) 范围内）
 * @return    成功返回句柄，失败返回 NULL（id 超出范围）
 */
tf_drv_io_handle_t tf_drv_io_register(int id);

/**
 * @brief 设置 IO 输出电平
 *
 * @param h      设备句柄
 * @param level  电平值：0=低电平 / 1=高电平
 */
void tf_drv_io_set(tf_drv_io_handle_t h, uint8_t level);

/**
 * @brief 读取 IO 输入电平
 *
 * @param h  设备句柄
 * @return   电平值：0=低电平 / 1=高电平
 */
uint8_t tf_drv_io_get(tf_drv_io_handle_t h);

/**
 * @brief 翻转 IO 输出电平
 *
 * 基于内部记录的 current_state 翻转，不重新读取硬件。
 *
 * @param h  设备句柄
 */
void tf_drv_io_toggle(tf_drv_io_handle_t h);

/**
 * @brief 根据枚举 ID 获取已注册的设备句柄
 *
 * @param id  IO 枚举 ID
 * @return    句柄（未注册或无效 ID 返回 NULL）
 */
tf_drv_io_handle_t tf_drv_io_get_handle(int id);

/**
 * @brief 检查 IO 设备是否已注册
 *
 * @param id  IO 枚举 ID
 * @return    true=已注册 / false=未注册
 */
bool tf_drv_io_is_registered(int id);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_IO_H */
