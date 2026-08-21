#ifndef TF_MODBUS_XM7828_H
#define TF_MODBUS_XM7828_H

#include "tf_modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM7828_TEMP_MIN_CENTI_C             (-3000)
#define XM7828_TEMP_MAX_CENTI_C             8500
#define XM7828_HUMIDITY_MAX_CENTI_RH        10000U
#define XM7828_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM7828 温湿度传感器数据
 *        解析后缓存的传感器测量值。
 *        温度单位：百分之一摄氏度；湿度单位：百分之一相对湿度。
 */
typedef struct {
    uint8_t slave_addr;                            /**< 从机 Modbus 地址              */
    tf_modbus_dev_status_t status;                 /**< 设备运行状态（在线、传感器故障）*/
    int16_t temperature_centi_c;                   /**< 温度（百分之一 °C），有符号   */
    uint16_t humidity_centi_rh;                    /**< 湿度（百分之一 %RH），无符号  */
} xm7828_th_data_t;

modbus_rtu_status_t tf_modbus_xm7828_parse(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           xm7828_th_data_t *out);
bool tf_modbus_xm7828_read(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_modbus_xm7828_write(uint8_t slave_addr, uint16_t reg,
                            uint16_t value);
modbus_rtu_status_t tf_modbus_xm7828_parse_write_ack(const uint8_t *frame,
                                                     uint32_t len,
                                                     uint8_t slave_addr,
                                                     uint8_t function,
                                                     uint16_t reg,
                                                     uint16_t value,
                                                     xm7828_th_data_t *out);
bool tf_modbus_xm7828_get(xm7828_th_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_MODBUS_XM7828_H */
