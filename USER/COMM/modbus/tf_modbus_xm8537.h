#ifndef TF_MODBUS_XM8537_H
#define TF_MODBUS_XM8537_H

#include "tf_modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8537_WATER_STATE_NORMAL           0x0000U
#define XM8537_WATER_STATE_DETECTED         0x0001U
#define XM8537_MEASUREMENT_REG_COUNT        1U

/**
 * @brief XM8537 水浸传感器数据
 *        raw：0x0000 = 无水（正常），0x0001 = 水浸（告警）
 */
typedef struct {
    uint8_t slave_addr;                            /**< 从机 Modbus 地址              */
    tf_modbus_dev_status_t status;                 /**< 设备运行状态（在线、传感器故障）*/
    uint16_t raw;                                  /**< 原始寄存器值                  */
    bool water_detected;                           /**< 水浸检测结果标志              */
} xm8537_water_data_t;

modbus_rtu_status_t tf_modbus_xm8537_parse(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           xm8537_water_data_t *out);
bool tf_modbus_xm8537_read(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_modbus_xm8537_write(uint8_t slave_addr, uint16_t reg,
                            uint16_t value);
modbus_rtu_status_t tf_modbus_xm8537_parse_write_ack(const uint8_t *frame,
                                                     uint32_t len,
                                                     uint8_t slave_addr,
                                                     uint8_t function,
                                                     uint16_t reg,
                                                     uint16_t value,
                                                     xm8537_water_data_t *out);
bool tf_modbus_xm8537_get(xm8537_water_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_MODBUS_XM8537_H */
