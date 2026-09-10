#include "COMM/modbus/devices/modbus_weather.h"

#include <string.h>
#include "COMM/modbus/modbus_rtu.h"

uint32_t modbus_weather_build_fault_notification(uint8_t addr, uint32_t fault_value, uint8_t *frame, uint32_t capacity)
{
    if (addr == 0U || addr > 247U || frame == NULL || capacity < MODBUS_WEATHER_FAULT_FRAME_LEN) return 0U;
    frame[0] = addr;
    frame[1] = MODBUS_WEATHER_FAULT_FUNCTION;
    frame[2] = MODBUS_WEATHER_FAULT_DATA_LEN;
    /* GD32 为小端；memcpy 避免 data 起点未按 uint32_t 对齐的访问。 */
    memcpy(&frame[3], &fault_value, sizeof(fault_value));
    modbus_rtu_append_crc(frame, MODBUS_WEATHER_FAULT_FRAME_LEN - 2U);
    return MODBUS_WEATHER_FAULT_FRAME_LEN;
}
