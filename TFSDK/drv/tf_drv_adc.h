/**
 * @file    tf_drv_adc.h
 * @brief   ADC 外设配置与转换控制
 *
 * ADC（模数转换器）完成模拟→数字转换。
 * TF SDK 统一的是 `config + register + handle + runtime op` 路径，
 * 初始化参数（alignment/scan_mode/continuous_mode/trigger/sample_time 等）
 * 已移入 HAL 层 per-instance 回调，DRV config 只保留资源描述。
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_adc_id_e 枚举（含 E_ADC_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_adc_config_t 配置数组（命名 adc_configs）
 * 3. 调用 tf_drv_adc_register() 完成依赖注册和 ADC 初始化
 * 4. 触发转换（软件触发或等待外部触发）
 * 5. 读取结果（regular_data_read / inserted_data_read）
 *
 * 示例——单通道轮询：
 * @code
 *   typedef enum { E_ADC_NONE=-1, E_ADC_0, E_ADC_MAX } tf_drv_adc_id_e;
 *
 *   adc_configs[E_ADC_0] = {
 *       .base     = (void *)ADC0,
 *       .ch_count = 1,
 *       .ch = {
 *           [0] = { .io_id = E_IO_ADC0_CH1, .channel = ADC_CHANNEL_1 },
 *       },
 *   };
 *
 *   tf_drv_adc_handle_t adc_h = tf_drv_adc_register(E_ADC_0);
 *   tf_drv_adc_software_trigger_enable(adc_h, TF_ADC_GROUP_REGULAR);
 *   while (!(tf_drv_adc_flag_get(adc_h, ADC_FLAG_EOC)));
 *   uint16_t val = tf_drv_adc_regular_data_read(adc_h);
 * @endcode
 *
 */

#ifndef TF_DRV_ADC_H
#define TF_DRV_ADC_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 



#ifdef __cplusplus
extern "C" {
#endif


/* ========== ADC 组定义 ========== */

#define TF_ADC_GROUP_REGULAR            0x01U  /**< 常规组 */
#define TF_ADC_GROUP_INSERTED           0x02U  /**< 注入组 */

/* ========== 类型定义 ========== */

/**
 * @brief ADC 通道配置——只保留资源描述
 *
 * 采样时间/偏移量等初始化参数由 HAL 层 per-channel 回调提供。
 */
typedef struct {
    int      io_id;      /**< 外部模拟输入对应的 IO ID，内部通道填 -1 */
    uint32_t channel;    /**< 平台原生通道宏（如 ADC_CHANNEL_1） */
    bool     enabled;    /**< 通道是否启用 */
} tf_drv_adc_ch_cfg_t;

enum { TF_DRV_ADC_CFG_CH_CAPACITY = 16 };

/**
 * @brief ADC 配置结构体——只保留资源描述字段
 *
 * 初始化参数（alignment/scan_mode/continuous_mode/trigger/sample_time 等）
 * 由 HAL 层 per-instance 回调提供。
 */
typedef struct {
    void                *base;      /**< ADC 实例（如 ADC1 / ADC2） */
    uint8_t              ch_count;  /**< 有效通道数 */
    tf_drv_adc_ch_cfg_t  ch[TF_DRV_ADC_CFG_CH_CAPACITY];  /**< 通道配置 */
} tf_drv_adc_config_t;

#if MCU_CONTAIN_PRE_ADC
extern tf_drv_adc_config_t adc_configs[];
#endif

/**
 * @brief ADC 设备句柄（不透明指针）
 */
typedef void * tf_drv_adc_handle_t;
void tf_drv_adc_software_trigger_enable(tf_drv_adc_handle_t h, uint8_t group);
uint16_t tf_drv_adc_regular_data_read(tf_drv_adc_handle_t h);
uint16_t tf_drv_adc_inserted_data_read(tf_drv_adc_handle_t h, uint8_t channel);

/* ========== 对外接口 ========== */

/**
 * @brief 注册 ADC 设备
 */
tf_drv_adc_handle_t tf_drv_adc_register(int id);

/**
 * @brief 获取句柄
 */
tf_drv_adc_handle_t tf_drv_adc_get_handle(int id);

/**
 * @brief 检查是否已注册
 */
bool tf_drv_adc_is_registered(int id);

/* ---- 标志位 ---- */

/**
 * @brief 获取 ADC 标志位
 *
 * @param flag 平台原生 ADC 标志位宏
 */
uint32_t tf_drv_adc_flag_get(tf_drv_adc_handle_t h, uint32_t flag);

/**
 * @brief 清除 ADC 标志位
 *
 * @param flag 平台原生 ADC 标志位宏
 */
void tf_drv_adc_flag_clear(tf_drv_adc_handle_t h, uint32_t flag);

#ifdef __cplusplus
}
#endif


#endif /* TF_DRV_ADC_H */
