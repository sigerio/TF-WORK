#ifndef TF_MODBUS_XM8189_H
#define TF_MODBUS_XM8189_H

#include "tf_modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8189_SPEED_MAX_CENTI_MS           3000U
#define XM8189_DIRECTION_MAX_CENTI_DEG      36000U
#define XM8189_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM8189 风速风向传感器数据
 *        速度单位：百分之一 m/s；风向单位：百分之一度。
 */
typedef struct {
    uint8_t slave_addr;                               /**< 从机 Modbus 地址              */
    tf_modbus_dev_status_t status;                    /**< 设备运行状态（在线、传感器故障）*/
    uint16_t speed_centi;                             /**< 风速（百分之一 m/s），0~3000 */
    uint16_t direction_centi_deg;                     /**< 风向（百分之一度），0~36000  */
} xm8189_wind_data_t;

modbus_rtu_status_t tf_modbus_xm8189_parse(const uint8_t *frame,
                                           uint32_t len,
                                           uint8_t slave_addr,
                                           xm8189_wind_data_t *out);
bool tf_modbus_xm8189_read(uint8_t slave_addr, uint16_t start_reg,
                           uint16_t reg_count);
bool tf_modbus_xm8189_write(uint8_t slave_addr, uint16_t reg,
                            uint16_t value);
modbus_rtu_status_t tf_modbus_xm8189_parse_write_ack(const uint8_t *frame,
                                                     uint32_t len,
                                                     uint8_t slave_addr,
                                                     uint8_t function,
                                                     uint16_t reg,
                                                     uint16_t value,
                                                     xm8189_wind_data_t *out);
bool tf_modbus_xm8189_get(xm8189_wind_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TF_MODBUS_XM8189_H */
