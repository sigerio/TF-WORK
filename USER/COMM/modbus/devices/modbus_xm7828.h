#ifndef MODBUS_XM7828_H
#define MODBUS_XM7828_H

#include "COMM/modbus/devices/modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM7828_TEMP_MIN_CENTI_C             (-3000)
#define XM7828_TEMP_MAX_CENTI_C             8500
#define XM7828_HUMIDITY_MAX_CENTI_RH        10000U
#define XM7828_REG_TEMP_CALIBRATION          0x006BU
#define XM7828_REG_HUMIDITY_CALIBRATION      0x006CU
#define XM7828_CALIBRATION_REG_COUNT         2U
#define XM7828_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM7828 温湿度传感器数据
 *        解析后缓存的传感器测量值。
 *        温度单位：百分之一摄氏度；湿度单位：百分之一相对湿度。
 */
typedef struct {
    uint8_t slave_addr;                            /**< 从机 Modbus 地址              */
    modbus_dev_status_t status;                 /**< 设备运行状态（在线、传感器故障）*/
    int16_t temperature_centi_c;                   /**< 温度（百分之一 °C），有符号   */
    uint16_t humidity_centi_rh;                    /**< 湿度（百分之一 %RH），无符号  */
    int16_t temperature_calibration_centi_c;       /**< 温度偏移（0.01 °C）           */
    int16_t humidity_calibration_centi_rh;         /**< 湿度偏移（0.01 %RH）          */
    uint8_t calibration_valid;                      /**< bit0 温度、bit1 湿度校正值有效 */
} xm7828_th_data_t;

/* 组帧：只写调用方缓冲，返回帧长；参数无效或空间不足返回 0。 */
uint32_t modbus_xm7828_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_xm7828_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity);

/* 解析：校验响应并更新调用方数据，不访问队列、硬件或全局缓存。 */
modbus_rtu_status_t modbus_xm7828_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm7828_th_data_t *out);
modbus_rtu_status_t modbus_xm7828_parse_calibration(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm7828_th_data_t *out);
modbus_rtu_status_t modbus_xm7828_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm7828_th_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_XM7828_H */
