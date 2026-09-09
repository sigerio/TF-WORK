#ifndef MODBUS_XM8189_H
#define MODBUS_XM8189_H

#include "COMM/modbus/devices/modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8189_SPEED_MAX_CENTI_MS           3000U
#define XM8189_DIRECTION_MAX_CENTI_DEG      36000U
#define XM8189_REG_SPEED_CALIBRATION         0x006BU
#define XM8189_REG_DIRECTION_CALIBRATION     0x006CU
#define XM8189_CALIBRATION_REG_COUNT         2U
#define XM8189_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM8189 风速风向传感器数据
 *        速度单位：百分之一 m/s；风向单位：百分之一度。
 */
typedef struct {
    uint8_t slave_addr;                               /**< 从机 Modbus 地址              */
    modbus_dev_status_t status;                    /**< 设备运行状态（在线、传感器故障）*/
    uint16_t speed_centi;                             /**< 风速（百分之一 m/s），0~3000 */
    uint16_t direction_centi_deg;                     /**< 风向（百分之一度），0~36000  */
    int16_t speed_calibration_centi_ms;               /**< 风速偏移（0.01 m/s）         */
    int16_t direction_calibration_centi_deg;          /**< 风向偏移（0.01 度）          */
    uint8_t calibration_valid;                         /**< bit0 风速、bit1 风向校正有效 */
} xm8189_wind_data_t;

/* 组帧：只写调用方缓冲，返回帧长；参数无效或空间不足返回 0。 */
uint32_t modbus_xm8189_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_xm8189_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity);

/* 解析：校验响应并更新调用方数据，不访问队列、硬件或全局缓存。 */
modbus_rtu_status_t modbus_xm8189_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8189_wind_data_t *out);
modbus_rtu_status_t modbus_xm8189_parse_calibration(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8189_wind_data_t *out);
modbus_rtu_status_t modbus_xm8189_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm8189_wind_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_XM8189_H */
