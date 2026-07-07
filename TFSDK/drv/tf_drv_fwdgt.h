/**
 * @file    tf_drv_fwdgt.h
 * @brief   FWDGT 自由看门狗配置与控制
 *
 * FWDGT 使用独立时钟（IRC40K 或 LSI），在系统主时钟失效时仍可触发复位。
 * 超时参数由 HAL 层硬编码（默认 ~1.5-1.9s），不通过公共 config 传入。
 *
 * ============ 使用流程 ============
 *
 * 1. 定义 tf_drv_fwdgt_id_e 枚举
 * 2. 调用 tf_drv_fwdgt_register() 启动看门狗
 * 3. 主循环定期调用 tf_drv_fwdgt_feed() 喂狗
 *
 * 示例：
 * @code
 *   typedef enum { E_FWDGT_0, E_FWDGT_MAX } tf_drv_fwdgt_id_e;
 *
 *   tf_drv_fwdgt_register(E_FWDGT_0);
 *   // 主循环中：
 *   tf_drv_fwdgt_feed(fwdgt_h);
 * @endcode
 *
 * ============ 架构说明 ============
 *
 * FWDGT 超时参数（prescaler/reload）已移入 HAL 层，
 * 各平台 HAL 在 tf_hal_fwdgt_init() 内以硬编码提供。
 * 需要修改超时时间时，编辑对应平台 tf_hal_fwdgt.c 中的参数。
 */

#ifndef TF_DRV_FWDGT_H
#define TF_DRV_FWDGT_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 

#ifdef __cplusplus
extern "C" {
#endif


/* ========== 类型定义 ========== */

/**
 * @brief FWDGT 配置结构体——只保留资源描述
 *
 * 超时参数（prescaler/reload）已移入 HAL 层硬编码。
 */
typedef struct {
    uint32_t _placeholder;      /**< 保留字段，当前未使用 */
} tf_drv_fwdgt_config_t;

#if MCU_CONTAIN_PRE_IWDG
extern tf_drv_fwdgt_config_t fwdgt_configs[];
#endif

/**
 * @brief FWDGT 句柄（不透明指针）
 */
typedef void * tf_drv_fwdgt_handle_t;
void tf_drv_fwdgt_feed(tf_drv_fwdgt_handle_t h);

/* ========== 对外接口 ========== */

/**
 * @brief 初始化并启动 FWDGT
 *
 * 各平台 HAL 内部以硬编码超时参数（~1.5-1.9s）配置并启动看门狗。
 * 启动后不可停止（直到系统复位）。
 *
 * @param id  FWDGT 枚举 ID
 * @return    成功返回句柄，失败返回 NULL
 */
tf_drv_fwdgt_handle_t tf_drv_fwdgt_register(int id);

tf_drv_fwdgt_handle_t tf_drv_fwdgt_get_handle(int id);
bool tf_drv_fwdgt_is_registered(int id);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_FWDGT_H */
