#ifndef TF_MODBUS_XM9505_H
#define TF_MODBUS_XM9505_H

#include "tf_modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM9505_REG_RAIN_TOTAL               0x0000U
#define XM9505_REG_RAIN_INSTANT             0x0001U
#define XM9505_REG_DIFF                     0x0002U
#define XM9505_REG_BASELINE                 0x0003U
#define XM9505_REG_RAINDROP_DYNAMIC         0x0004U
#define XM9505_REG_WATER_STATE              0x0005U
#define XM9505_REG_INTERVAL                 0x006AU
#define XM9505_REG_SENSITIVITY              0x006BU
#define XM9505_REG_WATER_THRESHOLD          0x006CU
#define XM9505_REG_HOUR_VALUE               0x006DU
#define XM9505_REG_12H_VALUE                0x006EU
#define XM9505_REG_ACCUMULATED              0x006FU
#define XM9505_RAIN_INSTANT_MAX_TENTH_MM    145U
#define XM9505_DIFF_MAX                     100U
#define XM9505_BASELINE_MAX                 10000U
#define XM9505_RAINDROP_DYNAMIC_MAX         1000U
#define XM9505_WATER_STATE_MAX              1000U
#define XM9505_MEASUREMENT_REG_COUNT        6U

/**
 * @brief XM9505 雨量传感器数据
 *        rain_count: 累计雨量翻斗计数（1 = 0.1mm）
 *        instant_tenth_mm: 瞬时雨量（0.1mm）  其余为各状态/校准原始值
 */
typedef struct {
    uint8_t slave_addr;                            /**< 从机 Modbus 地址              */
    tf_modbus_dev_status_t status;                 /**< 设备运行状态（在线、传感器故障）*/
    uint16_t rain_count;                           /**< 累计雨量翻斗计数（1 = 0.1mm）*/
    uint16_t instant_tenth_mm;                     /**< 瞬时雨量（0.1 mm）            */
    uint16_t diff;                                 /**< 差值/偏移值                   */
    uint16_t baseline;                             /**< 基线校准值                    */
    uint16_t raindrop_dynamic;                     /**< 雨滴动态检测值（0~1000）      */
    uint16_t water_state;                          /**< 雨水状态值（0~1000）          */
} xm9505_rain_data_t;

modbus_rtu_status_t tf_modbus_xm9505_parse(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           xm9505_rain_data_t *out);
bool tf_modbus_xm9505_read(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_modbus_xm9505_write(uint8_t slave_addr, uint16_t reg,
                            uint16_t value);
modbus_rtu_status_t tf_modbus_xm9505_parse_write_ack(const uint8_t *frame,
                                                     uint32_t len,
                                                     uint8_t slave_addr,
                                                     uint8_t function,
                                                     uint16_t reg,
                                                     uint16_t value,
                                                     xm9505_rain_data_t *out);
bool tf_modbus_xm9505_get(xm9505_rain_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_MODBUS_XM9505_H */
