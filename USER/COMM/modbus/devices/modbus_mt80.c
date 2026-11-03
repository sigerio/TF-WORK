#include "COMM/modbus/devices/modbus_mt80.h"

#include <stddef.h>
#include <string.h>

const mt80_register_t mt80_registers[MT80_REGISTER_COUNT] = {
    { MT80_REG_RATED_INVERTER_INPUT_VOLTAGE, offsetof(mt80_device_t, rated_inverter_input_voltage), 100U, 1U, 0U },
    { MT80_REG_RATED_INVERTER_POWER_L, offsetof(mt80_device_t, rated_inverter_power), 100U, 2U, 0U },
    { MT80_REG_RATED_PV_VOLTAGE, offsetof(mt80_device_t, rated_pv_voltage), 100U, 1U, 0U },
    { MT80_REG_RATED_PV_POWER_L, offsetof(mt80_device_t, rated_pv_power), 100U, 2U, 0U },
    { MT80_REG_RATED_PV_OUTPUT_VOLTAGE, offsetof(mt80_device_t, rated_pv_output_voltage), 100U, 1U, 0U },
    { MT80_REG_RATED_PV_OUTPUT_CURRENT, offsetof(mt80_device_t, rated_pv_output_current), 100U, 1U, 0U },
    { MT80_REG_INVERTER_VOLTAGE, offsetof(mt80_device_t, inverter_voltage), 100U, 1U, 0U },
    { MT80_REG_INVERTER_CURRENT, offsetof(mt80_device_t, inverter_current), 100U, 1U, 0U },
    { MT80_REG_INVERTER_STATE, offsetof(mt80_device_t, inverter_state), 1U, 1U, 0U },
    { MT80_REG_INVERTER_POWER_L, offsetof(mt80_device_t, inverter_power), 100U, 2U, 0U },
    { MT80_REG_INVERTER_FREQUENCY, offsetof(mt80_device_t, inverter_frequency), 100U, 1U, 0U },
    { MT80_REG_INVERTER_DAY_ENERGY_L, offsetof(mt80_device_t, inverter_day_energy), 100U, 2U, 0U },
    { MT80_REG_INVERTER_MONTH_ENERGY_L, offsetof(mt80_device_t, inverter_month_energy), 100U, 2U, 0U },
    { MT80_REG_INVERTER_YEAR_ENERGY_L, offsetof(mt80_device_t, inverter_year_energy), 100U, 2U, 0U },
    { MT80_REG_INVERTER_TOTAL_ENERGY_L, offsetof(mt80_device_t, inverter_total_energy), 100U, 2U, 0U },
    { MT80_REG_PV1_INPUT_VOLTAGE, offsetof(mt80_device_t, pv1_input_voltage), 100U, 1U, 0U },
    { MT80_REG_PV1_INPUT_CURRENT, offsetof(mt80_device_t, pv1_input_current), 100U, 1U, 0U },
    { MT80_REG_PV1_INPUT_POWER_L, offsetof(mt80_device_t, pv1_input_power), 100U, 2U, 0U },
    { MT80_REG_TODAY_GENERATION_L, offsetof(mt80_device_t, today_generation), 100U, 2U, 0U },
    { MT80_REG_MONTH_GENERATION_L, offsetof(mt80_device_t, month_generation), 100U, 2U, 0U },
    { MT80_REG_YEAR_GENERATION_L, offsetof(mt80_device_t, year_generation), 100U, 2U, 0U },
    { MT80_REG_TOTAL_GENERATION_L, offsetof(mt80_device_t, total_generation), 100U, 2U, 0U },
    { MT80_REG_CHARGE1_STATE, offsetof(mt80_device_t, charge1_state), 1U, 1U, 0U },
    { MT80_REG_LOAD_VOLTAGE, offsetof(mt80_device_t, load_voltage), 100U, 1U, 0U },
    { MT80_REG_LOAD_CURRENT, offsetof(mt80_device_t, load_current), 100U, 1U, 0U },
    { MT80_REG_LOAD_POWER_L, offsetof(mt80_device_t, load_power), 100U, 2U, 0U },
    { MT80_REG_LOAD_STATE, offsetof(mt80_device_t, load_state), 1U, 1U, 0U },
    { MT80_REG_TODAY_CONSUMPTION_L, offsetof(mt80_device_t, today_consumption), 100U, 2U, 0U },
    { MT80_REG_MONTH_CONSUMPTION_L, offsetof(mt80_device_t, month_consumption), 100U, 2U, 0U },
    { MT80_REG_YEAR_CONSUMPTION_L, offsetof(mt80_device_t, year_consumption), 100U, 2U, 0U },
    { MT80_REG_TOTAL_CONSUMPTION_L, offsetof(mt80_device_t, total_consumption), 100U, 2U, 0U },
    { MT80_REG_BATTERY_TEMPERATURE_RAW, offsetof(mt80_device_t, battery_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_BATTERY_VOLTAGE, offsetof(mt80_device_t, battery_voltage), 100U, 1U, 0U },
    { MT80_REG_BATTERY_CURRENT_L, offsetof(mt80_device_t, battery_current), 100U, 2U, 1U },
    { MT80_REG_BATTERY_SOC, offsetof(mt80_device_t, battery_soc), 1U, 1U, 0U },
    { MT80_REG_BATTERY_STATE, offsetof(mt80_device_t, battery_state), 1U, 1U, 0U },
    { MT80_REG_BATTERY_TYPE, offsetof(mt80_device_t, battery_type), 1U, 1U, 0U },
    { MT80_REG_BATTERY_CAPACITY, offsetof(mt80_device_t, battery_capacity), 1U, 1U, 0U },
    { MT80_REG_TEMPERATURE_COMPENSATION, offsetof(mt80_device_t, temperature_compensation), 1U, 1U, 0U },
    { MT80_REG_EQUALIZE_MINUTES, offsetof(mt80_device_t, equalize_minutes), 1U, 1U, 0U },
    { MT80_REG_BOOST_MINUTES, offsetof(mt80_device_t, boost_minutes), 1U, 1U, 0U },
    { MT80_REG_OVERVOLTAGE_DISCONNECT, offsetof(mt80_device_t, overvoltage_disconnect), 100U, 1U, 0U },
    { MT80_REG_CHARGE_VOLTAGE_LIMIT, offsetof(mt80_device_t, charge_voltage_limit), 100U, 1U, 0U },
    { MT80_REG_OVERVOLTAGE_RECONNECT, offsetof(mt80_device_t, overvoltage_reconnect), 100U, 1U, 0U },
    { MT80_REG_EQUALIZE_VOLTAGE, offsetof(mt80_device_t, equalize_voltage), 100U, 1U, 0U },
    { MT80_REG_BOOST_VOLTAGE, offsetof(mt80_device_t, boost_voltage), 100U, 1U, 0U },
    { MT80_REG_FLOAT_VOLTAGE, offsetof(mt80_device_t, float_voltage), 100U, 1U, 0U },
    { MT80_REG_BOOST_RECONNECT_VOLTAGE, offsetof(mt80_device_t, boost_reconnect_voltage), 100U, 1U, 0U },
    { MT80_REG_LOW_VOLTAGE_RECONNECT, offsetof(mt80_device_t, low_voltage_reconnect), 100U, 1U, 0U },
    { MT80_REG_UNDERVOLTAGE_ALARM_RECONNECT, offsetof(mt80_device_t, undervoltage_alarm_reconnect), 100U, 1U, 0U },
    { MT80_REG_UNDERVOLTAGE_ALARM, offsetof(mt80_device_t, undervoltage_alarm), 100U, 1U, 0U },
    { MT80_REG_LOW_VOLTAGE_DISCONNECT, offsetof(mt80_device_t, low_voltage_disconnect), 100U, 1U, 0U },
    { MT80_REG_DISCHARGE_VOLTAGE_LIMIT, offsetof(mt80_device_t, discharge_voltage_limit), 100U, 1U, 0U },
    { MT80_REG_LITHIUM_PROTECTION, offsetof(mt80_device_t, lithium_protection), 1U, 1U, 0U },
    { MT80_REG_LOW_CHARGE_TEMPERATURE_RAW, offsetof(mt80_device_t, low_charge_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_LOW_DISCHARGE_TEMPERATURE_RAW, offsetof(mt80_device_t, low_discharge_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_RTC_MINUTE_SECOND, offsetof(mt80_device_t, rtc_minute_second), 1U, 1U, 0U },
    { MT80_REG_INVERTER_VOLTAGE_SETTING, offsetof(mt80_device_t, inverter_voltage_setting), 100U, 1U, 0U },
    { MT80_REG_INVERTER_FREQUENCY_SETTING, offsetof(mt80_device_t, inverter_frequency_setting), 100U, 1U, 0U },
    { MT80_REG_INVERTER_LOW_VOLTAGE_DISCONNECT, offsetof(mt80_device_t, inverter_low_voltage_disconnect), 100U, 1U, 0U },
    { MT80_REG_INVERTER_LOW_VOLTAGE_RECONNECT, offsetof(mt80_device_t, inverter_low_voltage_reconnect), 100U, 1U, 0U },
    { MT80_REG_INVERTER_OVERVOLTAGE_RECONNECT, offsetof(mt80_device_t, inverter_overvoltage_reconnect), 100U, 1U, 0U },
    { MT80_REG_INVERTER_OVERVOLTAGE_DISCONNECT, offsetof(mt80_device_t, inverter_overvoltage_disconnect), 100U, 1U, 0U },
    { MT80_REG_BATTERY_HIGH_TEMPERATURE_RAW, offsetof(mt80_device_t, battery_high_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_BATTERY_LOW_TEMPERATURE_RAW, offsetof(mt80_device_t, battery_low_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_DEVICE_HIGH_TEMPERATURE_RAW, offsetof(mt80_device_t, device_high_temperature_raw), 100U, 1U, 1U },
    { MT80_REG_DEVICE_HIGH_TEMPERATURE_RECOVER_RAW, offsetof(mt80_device_t, device_high_temperature_recover_raw), 100U, 1U, 1U },
    { MT80_REG_LIGHT_ON_VOLTAGE, offsetof(mt80_device_t, light_on_voltage), 100U, 1U, 0U },
    { MT80_REG_LIGHT_ON_DELAY, offsetof(mt80_device_t, light_on_delay), 1U, 1U, 0U },
    { MT80_REG_LIGHT_OFF_VOLTAGE, offsetof(mt80_device_t, light_off_voltage), 100U, 1U, 0U },
    { MT80_REG_LIGHT_OFF_DELAY, offsetof(mt80_device_t, light_off_delay), 1U, 1U, 0U },
    { MT80_REG_LOAD_MODE, offsetof(mt80_device_t, load_mode), 1U, 1U, 0U },
    { MT80_REG_WORK_DURATION1, offsetof(mt80_device_t, work_duration1), 1U, 1U, 0U },
    { MT80_REG_WORK_DURATION2, offsetof(mt80_device_t, work_duration2), 1U, 1U, 0U },
    { MT80_REG_LOAD_ON_TIME1, offsetof(mt80_device_t, load_on_time1), 1U, 3U, 0U },
    { MT80_REG_LOAD_OFF_TIME1, offsetof(mt80_device_t, load_off_time1), 1U, 3U, 0U },
    { MT80_REG_LOAD_ON_TIME2, offsetof(mt80_device_t, load_on_time2), 1U, 3U, 0U },
    { MT80_REG_LOAD_OFF_TIME2, offsetof(mt80_device_t, load_off_time2), 1U, 3U, 0U },
    { MT80_REG_NIGHT_DURATION, offsetof(mt80_device_t, night_duration), 1U, 1U, 0U },
    { MT80_REG_SELECTED_PERIOD, offsetof(mt80_device_t, selected_period), 1U, 1U, 0U },
    { MT80_REG_MANUAL_DEFAULT_SWITCH, offsetof(mt80_device_t, manual_default_switch), 1U, 1U, 0U },
};

const uint16_t mt80_coils[MT80_COIL_COUNT] = {
    MT80_COIL_CLEAR_ENERGY, MT80_COIL_FAULT_RESET, MT80_COIL_INVERTER_OUTPUT,
    MT80_COIL_REMOTE_CONTROL, MT80_COIL_DC_OUTPUT
};

static bool mt80_range_valid(uint8_t function, uint16_t start, uint16_t count)
{
    uint32_t end = (uint32_t)start + count;
    if (count == 0U || end > 0x10000UL) return false;
    if (function == MODBUS_FC_READ_COILS) {
        return count == 1U && (start == MT80_COIL_CLEAR_ENERGY || start == MT80_COIL_FAULT_RESET ||
            start == MT80_COIL_INVERTER_OUTPUT || start == MT80_COIL_REMOTE_CONTROL || start == MT80_COIL_DC_OUTPUT);
    }
    if (count > MODULE_MODBUS_MT80_MAX_REGS) return false;
    if (function == MODBUS_FC_READ_HOLDING_REGS || function == MODBUS_FC_READ_INPUT_REGS) {
        uint32_t cursor = start;
        for (uint8_t i = 0U; i < MT80_REGISTER_COUNT && cursor < end; i++) {
            const mt80_register_t *point = &mt80_registers[i];
            if ((point->address >= 0x9600U) != (function == MODBUS_FC_READ_HOLDING_REGS)) continue;
            if (point->address != cursor) continue;
            cursor += point->words;
        }
        /* 不跨空洞；双字量与秒/分/时必须整组访问。 */
        return cursor == end;
    }
    return false;
}

uint32_t modbus_mt80_build_read_request(uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || !mt80_range_valid(function, start, count)) return 0U;
    return modbus_rtu_build_read_request(slave_addr, function, start, count, frame, capacity);
}

static bool mt80_write_value_valid(uint16_t reg, uint16_t value)
{
    if (reg >= MT80_REG_LOAD_ON_TIME1 && reg < MT80_REG_NIGHT_DURATION) {
        return value <= ((reg - MT80_REG_LOAD_ON_TIME1) % 3U == 2U ? 23U : 59U);
    }
    switch (reg) {
    case MT80_REG_BATTERY_TYPE: return value <= 0x0CU;
    case MT80_REG_TEMPERATURE_COMPENSATION: return value <= 9U;
    case MT80_REG_LITHIUM_PROTECTION: return value == 768U || value == 1024U;
    case MT80_REG_RTC_MINUTE_SECOND: return (value >> 8) <= 59U && (value & 0xFFU) <= 59U;
    case MT80_REG_INVERTER_VOLTAGE_SETTING:
        return value == 11000U || value == 12000U || value == 22000U || value == 23000U;
    case MT80_REG_INVERTER_FREQUENCY_SETTING: return value == 5000U || value == 6000U;
    case MT80_REG_LOAD_MODE: return value <= 5U;
    case MT80_REG_MANUAL_DEFAULT_SWITCH: return value <= 1U;
    case MT80_REG_WORK_DURATION1: case MT80_REG_WORK_DURATION2: case MT80_REG_NIGHT_DURATION:
        return (value & 0xFFU) <= 59U;
    case MT80_REG_LOW_CHARGE_TEMPERATURE_RAW: case MT80_REG_LOW_DISCHARGE_TEMPERATURE_RAW: {
        int32_t temperature = (value & 0x8000U) ? (int32_t)value - 65536 : (int32_t)value;
        return temperature >= -4000 && temperature <= 1000;
    }
    default: return true; /* 地址已过白名单；表中未给的数值范围由设备检查。 */
    }
}

uint32_t modbus_mt80_build_write_params(uint8_t slave_addr, uint16_t start, const uint16_t *values, uint16_t count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || values == 0 ||
        !mt80_range_valid(MODBUS_FC_READ_HOLDING_REGS, start, count)) return 0U;
    for (uint16_t i = 0; i < count; i++) {
        if (!mt80_write_value_valid((uint16_t)(start + i), values[i])) return 0U;
    }
    return modbus_rtu_build_write_regs_request(slave_addr, start, values, count, frame, capacity);
}

uint32_t modbus_mt80_build_write_coil(uint8_t slave_addr, uint16_t coil, bool on, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || !mt80_range_valid(MODBUS_FC_READ_COILS, coil, 1U)) return 0U;
    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_COIL, coil, on ? 0xFF00U : 0U, frame, capacity);
}

modbus_rtu_status_t modbus_mt80_parse_read(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, mt80_device_t *out)
{
    uint16_t regs[MODULE_MODBUS_MT80_MAX_REGS];
    uint16_t received;
    modbus_rtu_status_t status;

    if (out == 0 || slave_addr == 0U || slave_addr > 247U || !mt80_range_valid(function, start, count)) return MODBUS_RTU_ERR_PARAM;
    if (function == MODBUS_FC_READ_COILS) {
        uint8_t bit;
        status = modbus_rtu_parse_read_bits_response(frame, len, slave_addr, function, &bit, 1U, &received);
        if (status != MODBUS_RTU_OK) return status;
        if (frame[2] != 1U || received != 1U) return MODBUS_RTU_ERR_BYTE_COUNT;
        switch (start) {
        case MT80_COIL_CLEAR_ENERGY: out->clear_energy = bit != 0U; break;
        case MT80_COIL_FAULT_RESET: out->fault_reset = bit != 0U; break;
        case MT80_COIL_INVERTER_OUTPUT: out->inverter_output = bit != 0U; break;
        case MT80_COIL_REMOTE_CONTROL: out->remote_control = bit != 0U; break;
        case MT80_COIL_DC_OUTPUT: out->dc_output = bit != 0U; break;
        }
        for (uint8_t index = 0U; index < MT80_COIL_COUNT; index++) {
            if (mt80_coils[index] == start) out->coil_valid |= (uint8_t)(1U << index);
        }
    } else {
        status = modbus_rtu_parse_read_regs_response(frame, len, slave_addr, function, regs, count, &received);
        if (status != MODBUS_RTU_OK) return status;
        if (received != count) return MODBUS_RTU_ERR_BYTE_COUNT;
        /* 先校验整帧时间字段，错误响应不能部分覆盖缓存。 */
        for (uint16_t i = 0U; i < count; i++) {
            uint16_t reg = start + i;
            if (reg >= MT80_REG_LOAD_ON_TIME1 && reg < MT80_REG_NIGHT_DURATION &&
                !mt80_write_value_valid(reg, regs[i])) return MODBUS_RTU_ERR_VALUE;
        }
        for (uint8_t index = 0U; index < MT80_REGISTER_COUNT; index++) {
            const mt80_register_t *point = &mt80_registers[index];
            if (point->address < start || point->address >= (uint32_t)start + count) continue;
            uint16_t offset = point->address - start;
            if (point->words == 3U) {
                uint32_t seconds = regs[offset] + (uint32_t)regs[offset + 1U] * 60U + (uint32_t)regs[offset + 2U] * 3600U;
                memcpy((uint8_t *)out + point->offset, &seconds, sizeof(seconds));
            } else if (point->words == 2U) {
                uint32_t raw = (uint32_t)regs[offset] | ((uint32_t)regs[offset + 1U] << 16U);
                memcpy((uint8_t *)out + point->offset, &raw, sizeof(raw));
            } else {
                memcpy((uint8_t *)out + point->offset, &regs[offset], sizeof(regs[offset]));
            }
            out->register_valid[index] = 1U;
        }
    }
    out->slave_addr = slave_addr;
    return MODBUS_RTU_OK;
}
