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

uint32_t modbus_weather_sensor_build_read(uint8_t slave_addr, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || start != WEATHER_SENSOR_REG_START || count != WEATHER_SENSOR_REG_COUNT) return 0U;
    return modbus_rtu_build_read_request(slave_addr, MODBUS_FC_READ_HOLDING_REGS, start, count, frame, capacity);
}

uint32_t modbus_weather_sensor_build_write(uint8_t slave_addr, uint16_t reg, uint16_t value, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U) return 0U;
    switch (reg) {
    case WEATHER_SENSOR_REG_DIRECTION_OFFSET: if (value > 1U) return 0U; break;
    case WEATHER_SENSOR_REG_SPEED_ZERO: if (value != 0xAAU) return 0U; break;
    case WEATHER_SENSOR_REG_RAIN_ZERO: if (value != 0x5AU) return 0U; break;
    case WEATHER_SENSOR_REG_RAIN_SENSITIVITY: break; /* 协议未列范围，交由设备检查。 */
    default: return 0U;
    }
    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG, reg, value, frame, capacity);
}

modbus_rtu_status_t modbus_weather_sensor_parse(const uint8_t *frame, uint32_t len, uint8_t slave_addr, weather_sensor_data_t *out)
{
    uint16_t regs[WEATHER_SENSOR_REG_COUNT], received;
    modbus_rtu_status_t status;
    if (out == NULL || slave_addr == 0U || slave_addr > 247U) return MODBUS_RTU_ERR_PARAM;
    status = modbus_rtu_parse_read_regs_response(frame, len, slave_addr, MODBUS_FC_READ_HOLDING_REGS,
        regs, WEATHER_SENSOR_REG_COUNT, &received);
    if (status != MODBUS_RTU_OK) return status;
    if (received != WEATHER_SENSOR_REG_COUNT) return MODBUS_RTU_ERR_BYTE_COUNT;
    out->slave_addr = slave_addr;
    out->speed_deci_ms = regs[0];
    out->wind_force = regs[1];
    out->direction_sector = regs[2];
    out->direction_deg = regs[3];
    out->humidity_deci_rh = regs[4];
    out->temperature_deci_c = (int16_t)((regs[5] & 0x8000U) ? (int32_t)regs[5] - 65536 : regs[5]);
    out->rain_deci_mm = regs[13];
    out->measurement_valid = true;
    out->th_fault = out->humidity_deci_rh > 1000U;
    out->wind_fault = out->direction_deg > 360U || out->direction_sector > 7U;
    out->status.sensor_fault = out->th_fault || out->wind_fault;
    return MODBUS_RTU_OK;
}
