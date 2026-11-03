#ifndef MODBUS_WEATHER_H
#define MODBUS_WEATHER_H

#include "COMM/modbus/devices/modbus_common.h"

#define MODBUS_WEATHER_FAULT_FUNCTION 0x41U /* 用户自定义功能：单向故障状态通知，无应答。 */
#define MODBUS_WEATHER_FAULT_DATA_LEN 4U
#define MODBUS_WEATHER_FAULT_FRAME_LEN 9U

/* addr 41 04 data[4] CRC低 CRC高；data 使用 GD32 小端 uint32_t 内存字节序。
   仅组帧，不读应用状态、不发送；无效参数返回 0。 */
uint32_t modbus_weather_build_fault_notification(uint8_t addr, uint32_t fault_value, uint8_t *frame, uint32_t capacity);

/* 三合一设备：500..513 一次读取，保留协议原始精度；中间可选测量不对外解释。 */
#define WEATHER_SENSOR_REG_START 500U
#define WEATHER_SENSOR_REG_COUNT 14U
#define WEATHER_SENSOR_REG_DIRECTION_OFFSET 0x6000U
#define WEATHER_SENSOR_REG_SPEED_ZERO 0x6001U
#define WEATHER_SENSOR_REG_RAIN_ZERO 0x6002U
#define WEATHER_SENSOR_REG_RAIN_SENSITIVITY 0x6003U

typedef struct {
    uint8_t slave_addr;
    modbus_dev_status_t status;
    bool measurement_valid;
    bool th_fault;
    bool wind_fault;
    uint16_t speed_deci_ms;
    uint16_t wind_force;
    uint16_t direction_sector;
    uint16_t direction_deg;
    uint16_t humidity_deci_rh;
    int16_t temperature_deci_c;
    uint16_t rain_deci_mm;
} weather_sensor_data_t;

uint32_t modbus_weather_sensor_build_read(uint8_t slave_addr, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity);
uint32_t modbus_weather_sensor_build_write(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity);
modbus_rtu_status_t modbus_weather_sensor_parse(const uint8_t *frame, uint32_t len, uint8_t slave_addr, weather_sensor_data_t *out);

#endif /* MODBUS_WEATHER_H */
