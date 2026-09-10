#ifndef MODBUS_WEATHER_H
#define MODBUS_WEATHER_H

#include <stdint.h>

#define MODBUS_WEATHER_FAULT_FUNCTION 0x41U /* 用户自定义功能：单向故障状态通知，无应答。 */
#define MODBUS_WEATHER_FAULT_DATA_LEN 4U
#define MODBUS_WEATHER_FAULT_FRAME_LEN 9U

/* addr 41 04 data[4] CRC低 CRC高；data 使用 GD32 小端 uint32_t 内存字节序。
   仅组帧，不读应用状态、不发送；无效参数返回 0。 */
uint32_t modbus_weather_build_fault_notification(uint8_t addr, uint32_t fault_value, uint8_t *frame, uint32_t capacity);

#endif /* MODBUS_WEATHER_H */
