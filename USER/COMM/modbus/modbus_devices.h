#ifndef MODBUS_DEVICES_H
#define MODBUS_DEVICES_H

#include "COMM/modbus/devices/modbus_ac.h"
#include "COMM/modbus/devices/modbus_xm8537.h"
#include "COMM/modbus/devices/modbus_wanrong.h"
#include "COMM/modbus/devices/modbus_mt80.h"
#include "COMM/modbus/devices/modbus_weather.h"
#include "COMM/modbus/modbus_txq.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 每次推进一项巡检；B4 项触发优先槽补发，其余请求入普通队列。 */
void modbus_devices_poll(void);

/* 三种电机实例，写 4X-2000；物理按键有效或同方向限位时拒绝启动。
 * true 仅表示入队，未发运动可因物理接管/限位清除，亦可能被普通队列淘汰。 */
bool modbus_wanrong_queue_command(modbus_dev_type_t device, wanrong_command_e command);
/* 帧交给发送链路后记录运动方向；停止方向仅在有效停止回显后清除。 */
void modbus_wanrong_command_sent(const modbus_rsp_expect_t *expect);
/* MT80 使用配置的上行地址；只访问已定义寄存器，不跨空洞。 */
bool modbus_mt80_queue_read(uint8_t function, uint16_t start, uint16_t count);
bool modbus_mt80_queue_write_params(uint16_t start, const uint16_t *values, uint16_t count);
bool modbus_mt80_queue_write_coil(uint16_t coil, bool on);
/* 复制是否成功；status.online 仅为最近一次事务状态，无分段新鲜度保证。 */
bool modbus_mt80_get(mt80_device_t *out);

bool modbus_weather_sensor_get(weather_sensor_data_t *out);
bool modbus_weather_sensor_queue_write(uint16_t reg, uint16_t value);

/* 异步写请求：true 只表示已入队，执行结果由后续响应更新缓存。 */
bool modbus_xm8537_queue_write(uint8_t slave_addr, uint16_t reg, uint16_t value);
bool modbus_ac_queue_write_status(uint8_t slave_addr, uint16_t status_ref, bool on);
bool modbus_ac_queue_write_param(uint8_t slave_addr, uint16_t param_ref, int16_t value);

bool modbus_xm8537_get(xm8537_water_data_t *out);
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
