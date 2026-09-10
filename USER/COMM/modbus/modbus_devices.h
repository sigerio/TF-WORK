#ifndef MODBUS_DEVICES_H
#define MODBUS_DEVICES_H

#include "COMM/modbus/devices/modbus_ac.h"
#include "COMM/modbus/devices/modbus_xm7828.h"
#include "COMM/modbus/devices/modbus_xm8189.h"
#include "COMM/modbus/devices/modbus_xm8537.h"
#include "COMM/modbus/devices/modbus_xm9505.h"
#include "COMM/modbus/modbus_txq.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 每次推进一项巡检；B4 项触发优先槽补发，其余请求入普通队列。 */
void modbus_devices_poll(void);

/* 异步写请求：true 只表示已入队，执行结果由后续响应更新缓存。 */
bool modbus_xm7828_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value);
bool modbus_xm8189_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value);
bool modbus_xm8537_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value);
bool modbus_xm9505_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value);
bool modbus_ac_queue_write_status(uint8_t slave_addr, uint16_t status_ref, bool on);
bool modbus_ac_queue_write_param(uint8_t slave_addr, uint16_t param_ref, int16_t value);

/* 读取设备缓存。气象设备返回复制是否成功；空调返回在线或对应数据有效标记。 */
bool modbus_xm7828_get(xm7828_th_data_t *out);
bool modbus_xm8189_get(xm8189_wind_data_t *out);
bool modbus_xm8537_get(xm8537_water_data_t *out);
bool modbus_xm9505_get(xm9505_rain_data_t *out);
bool modbus_ac_get_params(ac_params_t *out);
bool modbus_ac_get_status(ac_status_t *out);
bool modbus_ac_get(ac_device_t *out);

/* 主站内部接口：初始化状态、更新通信结果、解析当前请求的响应。 */
void modbus_devices_init(void);
void modbus_devices_set_online(modbus_dev_type_t device, bool online);
modbus_rtu_status_t modbus_devices_parse_response(const modbus_rsp_expect_t *expect, const uint8_t *frame, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_DEVICES_H */
