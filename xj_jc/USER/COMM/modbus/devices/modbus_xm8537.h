#ifndef MODBUS_XM8537_H
#define MODBUS_XM8537_H

#include "COMM/modbus/devices/modbus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8537_WATER_STATE_NORMAL           0x0000U
#define XM8537_WATER_STATE_DETECTED         0x0001U
#define XM8537_MEASUREMENT_REG_COUNT        1U

/**
 * @brief XM8537 水浸传感器数据
 *        仅 0x0000（无水）、0x0001（水浸）合法，其他值置传感器故障并保留上次有效数据。
 */
typedef struct {
    uint8_t slave_addr;                            /**< 从机 Modbus 地址              */
    modbus_dev_status_t status;                 /**< 设备运行状态（在线、传感器故障）*/
    uint16_t raw;                                  /**< 原始寄存器值                  */
    bool water_detected;                           /**< 水浸检测结果标志              */
} xm8537_water_data_t;

/* 组帧：只写调用方缓冲，返回帧长；参数无效或空间不足返回 0。 */
uint32_t modbus_xm8537_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_xm8537_build_write_request(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity);

/* 解析：校验响应并更新调用方数据，不访问队列、硬件或全局缓存。 */
modbus_rtu_status_t modbus_xm8537_parse_measurement(const uint8_t *frame, uint32_t len, uint8_t slave_addr, xm8537_water_data_t *out);
modbus_rtu_status_t modbus_xm8537_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t reg, uint16_t value, xm8537_water_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_XM8537_H */
