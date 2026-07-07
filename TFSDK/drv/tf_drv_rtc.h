/**
 * @file    tf_drv_rtc.h
 * @brief   RTC 实时时钟配置与控制
 *
 * RTC（Real-Time Clock）使用 LXTAL（32.768kHz）外部晶振，
 * 提供秒计数、闹钟和中断功能。需配合 PMU + BKP 域操作。
 *
 * ============ 使用流程 ============
 *
 * 1. 定义 tf_drv_rtc_id_e 枚举
 * 2. 填写 rtc_configs[] 配置数组
 * 3. 调用 tf_drv_rtc_register() 完成 RTC 初始化
 * 4. 主循环读取当前时间：tf_drv_rtc_counter_get()
 *
 * 示例：
 * @code
 *   typedef enum { E_RTC_0, E_RTC_MAX } tf_drv_rtc_id_e;
 *
 *   rtc_configs[E_RTC_0] = {
 *       .prescaler       = 32767,             // 1s 周期
 *       .initial_counter = 86400,             // 从 00:00:00 起始
 *       .alarm_value     = 0,
 *   };
 *
 *   tf_drv_rtc_register(E_RTC_0);
 *
 *   // 主循环中：
 *   uint32_t now = tf_drv_rtc_counter_get(rtc_h);
 *   // now / 3600 = 小时, (now % 3600) / 60 = 分钟, now % 60 = 秒
 * @endcode
 */

#ifndef TF_DRV_RTC_H
#define TF_DRV_RTC_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 

#ifdef __cplusplus
extern "C" {
#endif


/* ========== 类型定义 ========== */

/**
 * @brief RTC 配置结构体——只保留资源描述
 *
 * 初始化参数（prescaler/initial_counter）由 HAL 层硬编码。
 */
typedef struct {
    uint32_t _placeholder;      /**< 保留字段，当前未使用 */
} tf_drv_rtc_config_t;

#if MCU_CONTAIN_PRE_RTC
extern tf_drv_rtc_config_t rtc_configs[];
#endif

/**
 * @brief RTC 句柄（不透明指针）
 */
typedef void * tf_drv_rtc_handle_t;

/* ========== 对外接口 ========== */

/**
 * @brief 初始化 RTC
 *
 * 完整 RTC 初始化序列：使能 LXTAL → 选择时钟源 → 配置模式 →
 * 设预分频 → 设初始值 → 退出配置模式 → 等待同步。
 *
 * @param id  RTC 枚举 ID
 * @return    成功返回句柄，失败返回 NULL
 */
tf_drv_rtc_handle_t tf_drv_rtc_register(int id);

tf_drv_rtc_handle_t tf_drv_rtc_get_handle(int id);
bool tf_drv_rtc_is_registered(int id);

/* ---- 计数器操作 ---- */

void     tf_drv_rtc_counter_set(tf_drv_rtc_handle_t h, uint32_t cnt);
uint32_t tf_drv_rtc_counter_get(tf_drv_rtc_handle_t h);

/* ---- 预分频 ---- */

void tf_drv_rtc_prescaler_set(tf_drv_rtc_handle_t h, uint32_t psc);

/* ---- 闹钟 ---- */

void tf_drv_rtc_alarm_config(tf_drv_rtc_handle_t h, uint32_t alarm);

#ifdef __cplusplus
}
#endif

#endif /* TF_DRV_RTC_H */
