/**
 * @file    tf_drv_rcu.h
 * @brief   RCU 系统时钟配置与管理
 *
 * RCU（Reset and Clock Unit）管理系统时钟源选择、PLL 倍频、总线分频。
 * 本模块为系统时钟的唯一入口，各平台 HAL 层自包含完整时钟配置。
 * 晶振、PLL 参数、总线分频等均在 HAL 层代码中固定，不通过配置表传入。
 *
 * ============ 使用流程 ============
 *
 * 1. 在 main 启动阶段最先调用 tf_drv_rcu_register()
 * 2. register() 内部调平台 HAL 完成全部时钟配置
 *
 * 示例：
 * @code
 *   int main(void) {
 *       HAL_Init();
 *       tf_drv_rcu_register(0);            // 系统时钟初始化
 *       MX_GPIO_Init();
 *       // ...
 *   }
 * @endcode
 */

#ifndef TF_DRV_RCU_H
#define TF_DRV_RCU_H

#include <stdint.h>
#include <stdbool.h>
 


#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

typedef enum {
    E_DRV_RCU_CLOCK_SYSCLK = 10, /**< SYSCLK 频率查询 ID */
    E_DRV_RCU_CLOCK_AHB    = 11, /**< AHB 总线频率查询 ID */
    E_DRV_RCU_CLOCK_APB1   = 12, /**< APB1 总线频率查询 ID */
    E_DRV_RCU_CLOCK_APB2   = 13, /**< APB2 总线频率查询 ID */
    E_DRV_RCU_CLOCK_APB3   = 14, /**< APB3 总线频率查询 ID（STM32H5 等支持） */
} tf_drv_rcu_clock_e;

/**
 * @brief RCU 句柄（不透明指针）
 */
typedef void * tf_drv_rcu_handle_t;

/* ========== 对外接口 ========== */

/**
 * @brief 初始化系统时钟
 *
 * 内部调用平台 HAL 的系统时钟配置入口。
 * 晶振、PLL 参数、总线分频等均在 HAL 层代码中固定，不通过配置表传入。
 *
 * @param id  RCU 枚举 ID（保留参数，当前固定填 0）
 * @return    成功返回句柄，失败返回 NULL
 */
tf_drv_rcu_handle_t tf_drv_rcu_register(int id);

/**
 * @brief 获取 RCU 句柄
 */
tf_drv_rcu_handle_t tf_drv_rcu_get_handle(int id);

/**
 * @brief 检查 RCU 是否已注册
 */
bool tf_drv_rcu_is_registered(int id);

/**
 * @brief 获取系统时钟频率
 *
 * @param clock_id  查询 ID（E_DRV_RCU_CLOCK_SYSCLK/AHB/APB1/APB2/APB3）
 * @return          频率（Hz），查询 ID 无效返回 0
 */
uint32_t tf_drv_rcu_clock_freq_get(tf_drv_rcu_clock_e clock_id);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_RCU_H */
