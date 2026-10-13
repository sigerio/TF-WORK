#include "COMM/modbus/devices/modbus_tis.h"

/* 二进制补码，避免依赖 uint16_t -> int16_t 越界转换。 */
static int16_t tis_signed_value(uint16_t raw)
{
    int32_t value = (raw & 0x8000U) ? (int32_t)raw - 65536 : (int32_t)raw;
    return (int16_t)value;
}

static bool tis_range_valid(uint8_t function, uint16_t start, uint16_t count)
{
    uint32_t end = (uint32_t)start + count;
    bool valid;

    if ((count == 0U) || (end > 0x10000UL)) return false;
    if (function == MODBUS_FC_READ_COILS) {
        return count <= MODULE_MODBUS_TIS_MAX_BITS &&
            (((start == TIS_COIL_CHARGE || start == TIS_COIL_MANUAL_OUTPUT) && count == 1U) ||
             (start >= TIS_COIL_FACTORY_RESET && end <= TIS_COIL_FAULT_RESET + 1U));
    }
    if (count > MODULE_MODBUS_TIS_MAX_REGS) return false;
    if (function == MODBUS_FC_READ_HOLDING_REGS) {
        static const uint16_t time_groups[] = {0x9019U, 0x902CU, 0x902FU, 0x9032U, 0x9035U};
        valid = (start >= 0x9000U && end <= 0x9044U) ||
            (start >= 0x9045U && end <= 0x9048U) || (start >= 0x9049U && end <= 0x904CU);
        if (!valid) return false;
        for (unsigned i = 0; i < sizeof(time_groups) / sizeof(time_groups[0]); i++) {
            uint16_t group = time_groups[i];
            if (start < group + 3U && end > group && (start > group || end < group + 3U)) return false;
        }
        return true;
    }
    if (function != MODBUS_FC_READ_INPUT_REGS) return false;
    valid = (start >= 0x3002U && end <= 0x300EU) || (start >= 0x3100U && end <= 0x311BU) ||
        (start >= 0x311DU && end <= 0x3122U) || (start >= 0x3200U && end <= 0x3204U) ||
        (start == 0x3205U && count == 1U) || (start >= 0x3300U && end <= 0x3315U) ||
        (start >= 0x3400U && end <= 0x3424U);
    if (!valid) return false;
    /* 一个 32 位工程量的两个寄存器必须在同一事务读取。 */
    static const uint16_t low_words[] = {
        0x3004U, 0x3008U, 0x300CU, 0x3102U, 0x3106U, 0x310AU, 0x310EU, 0x3112U, 0x3120U,
        0x3303U, 0x3305U, 0x3307U, 0x3309U, 0x330BU, 0x330DU, 0x330FU, 0x3311U, 0x3313U, 0x3403U
    };
    for (unsigned i = 0; i < sizeof(low_words) / sizeof(low_words[0]); i++) {
        if (start == low_words[i] + 1U || end == low_words[i] + 1U) return false;
    }
    return true;
}

uint32_t modbus_tis_build_read_request(uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || !tis_range_valid(function, start, count)) return 0U;
    return modbus_rtu_build_read_request(slave_addr, function, start, count, frame, capacity);
}

static bool tis_write_value_valid(uint16_t reg, uint16_t value, uint16_t voltage_level)
{
    switch (reg) {
    case 0x9000: return value <= 0x0CU; /* 表列 1..0x0C；User 的“000OH”暂按 0x0000，不扩展未定义类型。 */
    case 0x9001: return value >= 1U && value <= 4000U;
    case 0x9002: return value <= 900U;
    case 0x9003: return value <= 1U;
    case 0x9004: case 0x9005: return value <= 100U;
    case 0x9006: return value <= 4U;
    case 0x9007: case 0x9008: case 0x9009: case 0x900A: case 0x900B: case 0x900C:
    case 0x900E: case 0x900F: case 0x9010: case 0x9011: case 0x9012:
        return voltage_level >= 1U && voltage_level <= 4U && value >= 900U * voltage_level && value <= 1700U * voltage_level;
    case 0x900D: return true; /* 表未给独立范围，不从旧缓存推导。 */
    case 0x9013: return value >= 100U && value <= 10000U; /* 型号额定限流由设备执行。 */
    case 0x9014: case 0x9015: return value <= 120U;
    case 0x9016: return value == 3U || value == 4U;
    case 0x9017: case 0x9018: return tis_signed_value(value) >= -2500 && tis_signed_value(value) <= 1000;
    case 0x9019: return (value & 0xFFU) <= 59U && (value >> 8) <= 59U;
    case 0x901A: return (value & 0xFFU) <= 23U && (value >> 8) >= 1U && (value >> 8) <= 31U;
    case 0x901B: return (value & 0xFFU) >= 1U && (value & 0xFFU) <= 12U && (value >> 8) <= 99U;
    case 0x901C: return value >= 4000U && value <= 10000U;
    case 0x901D: return tis_signed_value(value) >= 0 && tis_signed_value(value) <= 6000;
    case 0x901E: case 0x9020: return value >= 6500U && value <= 10000U;
    case 0x901F: case 0x9021: return value >= 5500U && value <= 10000U;
    case 0x9022: return value <= 3U || value == 6U || value == 7U;
    case 0x9023: return value <= 1U;
    case 0x9024: return value >= 500U && value <= 5000U;
    case 0x9025: case 0x9027: return value <= 9900U;
    case 0x9026: return value >= 60U && value <= 8000U;
    case 0x9028:
        return (value >> 8) >= 3U && (value >> 8) <= 24U && (value & 0xFFU) <= 59U && (value <= 0x1800U);
    case 0x9029: return value <= 1U;
    case 0x902A: case 0x902B: return value <= 0x1800U && (value & 0xFFU) <= 59U;
    case 0x902C: case 0x902D: case 0x902F: case 0x9030:
    case 0x9032: case 0x9033: case 0x9035: case 0x9036: return value <= 59U;
    case 0x902E: case 0x9031: case 0x9034: case 0x9037: return value <= 23U;
    case 0x9038: return value <= 1U;
    case 0x9039: case 0x903A: case 0x903B: case 0x903C: case 0x903D: case 0x903E:
        return value >= 1U && value <= 100U;
    case 0x903F: return value >= 10U && value <= 120U;
    case 0x9040: return value >= 1U && value <= 230U;
    case 0x9041: case 0x9042: case 0x9043: return value <= 1U;
    case 0x9045: return value >= 1U && value <= 100U;
    case 0x9046: return value == 1152U || value == 96U || value == 24U;
    case 0x9047: return value >= 100U && value <= 1200U;
    case 0x9049: return value <= 60U;
    case 0x904A: case 0x904B: return value <= 1U;
    default: return false;
    }
}

uint32_t modbus_tis_build_write_params(uint8_t slave_addr, uint16_t start, const uint16_t *values, uint16_t count, uint16_t voltage_level, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || values == 0 ||
        !tis_range_valid(MODBUS_FC_READ_HOLDING_REGS, start, count)) return 0U;
    /* 同一请求显式设置非零系统等级时，以新值校验随后的电压参数。 */
    if (start <= TIS_REG_VOLTAGE_LEVEL && (uint32_t)start + count > TIS_REG_VOLTAGE_LEVEL &&
        values[TIS_REG_VOLTAGE_LEVEL - start] != 0U) {
        voltage_level = values[TIS_REG_VOLTAGE_LEVEL - start];
    }
    for (uint16_t i = 0; i < count; i++) {
        if (!tis_write_value_valid((uint16_t)(start + i), values[i], voltage_level)) return 0U;
    }
    return modbus_rtu_build_write_regs_request(slave_addr, start, values, count, frame, capacity);
}

uint32_t modbus_tis_build_write_coil(uint8_t slave_addr, uint16_t coil, bool on, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U || !tis_range_valid(MODBUS_FC_READ_COILS, coil, 1U)) return 0U;
    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_COIL, coil, on ? 0xFF00U : 0U, frame, capacity);
}

modbus_rtu_status_t modbus_tis_parse_read(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint8_t function, uint16_t start, uint16_t count, tis_device_t *out)
{
    uint16_t regs[MODULE_MODBUS_TIS_MAX_REGS];
    uint16_t received;
    modbus_rtu_status_t status;

    if (out == 0 || slave_addr == 0U || slave_addr > 247U || !tis_range_valid(function, start, count)) return MODBUS_RTU_ERR_PARAM;
    if (function == MODBUS_FC_READ_COILS) {
        uint8_t bits[MODULE_MODBUS_TIS_MAX_BITS];
        status = modbus_rtu_parse_read_bits_response(frame, len, slave_addr, function, bits, count, &received);
        if (status != MODBUS_RTU_OK) return status;
        if (frame[2] != (count + 7U) / 8U || received != count) return MODBUS_RTU_ERR_BYTE_COUNT;
        for (uint16_t i = 0; i < count; i++) {
            switch (start + i) {
            case TIS_COIL_CHARGE: out->charging_enabled = bits[i] != 0; break;
            case TIS_COIL_MANUAL_OUTPUT: out->manual_output_enabled = bits[i] != 0; break;
            case TIS_COIL_FACTORY_RESET: out->factory_reset = bits[i] != 0; break;
            case TIS_COIL_CLEAR_ENERGY: out->clear_energy = bits[i] != 0; break;
            case TIS_COIL_FAULT_RESET: out->fault_reset = bits[i] != 0; break;
            }
        }
    } else {
        status = modbus_rtu_parse_read_regs_response(frame, len, slave_addr, function, regs, count, &received);
        if (status != MODBUS_RTU_OK) return status;
        if (received != count) return MODBUS_RTU_ERR_BYTE_COUNT;
        for (uint16_t i = 0; i < count; i++) {
            uint16_t reg = (uint16_t)(start + i);
            switch (reg) {
            case TIS_REG_RATED_PV_VOLTAGE: out->rated_pv_voltage = regs[i]; break;
            case TIS_REG_RATED_PV_CURRENT: out->rated_pv_current = regs[i]; break;
            case TIS_REG_RATED_PV_POWER: out->rated_pv_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_RATED_BATTERY_VOLTAGE: out->rated_battery_voltage = regs[i]; break;
            case TIS_REG_RATED_CHARGE_CURRENT: out->rated_charge_current = regs[i]; break;
            case TIS_REG_RATED_CHARGE_POWER: out->rated_charge_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_RATED_LOAD_VOLTAGE: out->rated_load_voltage = regs[i]; break;
            case TIS_REG_RATED_LOAD_CURRENT: out->rated_load_current = regs[i]; break;
            case TIS_REG_RATED_LOAD_POWER: out->rated_load_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_PV1_INPUT_VOLTAGE: out->pv1_input_voltage = regs[i]; break;
            case TIS_REG_PV1_INPUT_CURRENT: out->pv1_input_current = regs[i]; break;
            case TIS_REG_PV1_INPUT_POWER: out->pv1_input_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_PV1_OUTPUT_VOLTAGE: out->pv1_output_voltage = regs[i]; break;
            case TIS_REG_PV1_OUTPUT_CURRENT: out->pv1_output_current = regs[i]; break;
            case TIS_REG_PV1_OUTPUT_POWER: out->pv1_output_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_PV2_INPUT_VOLTAGE: out->pv2_input_voltage = regs[i]; break;
            case TIS_REG_PV2_INPUT_CURRENT: out->pv2_input_current = regs[i]; break;
            case TIS_REG_PV2_INPUT_POWER: out->pv2_input_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_PV2_OUTPUT_VOLTAGE: out->pv2_output_voltage = regs[i]; break;
            case TIS_REG_PV2_OUTPUT_CURRENT: out->pv2_output_current = regs[i]; break;
            case TIS_REG_PV2_OUTPUT_POWER: out->pv2_output_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_LOAD_VOLTAGE: out->load_voltage = regs[i]; break;
            case TIS_REG_LOAD_CURRENT: out->load_current = regs[i]; break;
            case TIS_REG_LOAD_POWER: out->load_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_BATTERY_VOLTAGE: out->battery_voltage = regs[i]; break;
            case TIS_REG_BATTERY_CHARGE_CURRENT: out->battery_charge_current = regs[i]; break;
            case TIS_REG_BATTERY_DISCHARGE_CURRENT: out->battery_discharge_current = regs[i]; break;
            case TIS_REG_BATTERY_TOTAL_CURRENT: out->battery_total_current = regs[i]; break;
            case TIS_REG_BATTERY_TEMPERATURE: out->battery_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BATTERY_SOC: out->battery_soc = regs[i]; break;
            case TIS_REG_DEVICE_TEMPERATURE: out->device_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_SYSTEM_VOLTAGE: out->system_voltage = regs[i]; break;
            case TIS_REG_MAX_PV_VOLTAGE: out->max_pv_voltage = regs[i]; break;
            case TIS_REG_PV_CHARGE_CURRENT: out->pv_charge_current = regs[i]; break;
            case TIS_REG_PV_CHARGE_POWER: out->pv_charge_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_BATTERY_STATE: out->battery_state = regs[i]; break;
            case TIS_REG_LOAD_STATE: out->load_state = regs[i]; break;
            case TIS_REG_CHARGE1_STATE: out->charge1_state = regs[i]; break;
            case TIS_REG_CHARGE2_STATE: out->charge2_state = regs[i]; break;
            case TIS_REG_DEVICE_STATE: out->device_state = regs[i]; break;
            case TIS_REG_TODAY_MAX_PV_VOLTAGE: out->today_max_pv_voltage = regs[i]; break;
            case TIS_REG_TODAY_MAX_BATTERY_VOLTAGE: out->today_max_battery_voltage = regs[i]; break;
            case TIS_REG_TODAY_MIN_BATTERY_VOLTAGE: out->today_min_battery_voltage = regs[i]; break;
            case TIS_REG_TODAY_CONSUMPTION: out->today_consumption = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_MONTH_CONSUMPTION: out->month_consumption = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_YEAR_CONSUMPTION: out->year_consumption = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_TOTAL_CONSUMPTION: out->total_consumption = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_TODAY_GENERATION: out->today_generation = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_MONTH_GENERATION: out->month_generation = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_YEAR_GENERATION: out->year_generation = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_TOTAL_GENERATION: out->total_generation = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_LEARNED_CAPACITY: out->learned_capacity = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_BMS_CELL_COUNT: out->bms_cell_count = regs[i]; break;
            case TIS_REG_BMS_VOLTAGE: out->bms_voltage = regs[i]; break;
            case TIS_REG_BMS_CURRENT: out->bms_current = regs[i]; break;
            case TIS_REG_BMS_POWER: out->bms_power = ((uint32_t)regs[i + 1U] << 16) | regs[i]; i++; break;
            case TIS_REG_BMS_FULL_CAPACITY: out->bms_full_capacity = regs[i]; break;
            case TIS_REG_BMS_SOC: out->bms_soc = regs[i]; break;
            case TIS_REG_BMS_REMAINING_MINUTES: out->bms_remaining_minutes = regs[i]; break;
            case TIS_REG_BMS_MAX_CELL_TEMPERATURE: out->bms_max_cell_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BMS_MIN_CELL_TEMPERATURE: out->bms_min_cell_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BMS_BALANCE_TEMPERATURE: out->bms_balance_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BMS_AMBIENT_TEMPERATURE: out->bms_ambient_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BMS_MOS_TEMPERATURE: out->bms_mos_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_BMS_CYCLE_COUNT: out->bms_cycle_count = regs[i]; break;
            case TIS_REG_BMS_BALANCE_STATE: out->bms_balance_state = regs[i]; break;
            case TIS_REG_BMS_VOLTAGE_STATE: out->bms_voltage_state = regs[i]; break;
            case TIS_REG_BMS_CURRENT_STATE: out->bms_current_state = regs[i]; break;
            case TIS_REG_BMS_MOS_STATE: out->bms_mos_state = regs[i]; break;
            case TIS_REG_BMS_CELL_TEMPERATURE_STATE: out->bms_cell_temperature_state = regs[i]; break;
            case TIS_REG_BMS_BALANCE_TEMPERATURE_STATE: out->bms_balance_temperature_state = regs[i]; break;
            case TIS_REG_BMS_AMBIENT_TEMPERATURE_STATE: out->bms_ambient_temperature_state = regs[i]; break;
            case TIS_REG_BMS_MOS_TEMPERATURE_STATE: out->bms_mos_temperature_state = regs[i]; break;
            case TIS_REG_BMS_CELL_STATE: out->bms_cell_state = regs[i]; break;
            case TIS_REG_BMS_PROTOCOL: out->bms_protocol = regs[i]; break;
            case TIS_REG_BMS_BATTERY_STATE: out->bms_battery_state = regs[i]; break;
            case TIS_REG_BMS_PACK_MODE: out->bms_pack_mode = regs[i]; break;
            case TIS_REG_BMS_GROUP_VOLTAGE: out->bms_group_voltage = regs[i]; break;
            case TIS_REG_BMS_GROUP_CURRENT: out->bms_group_current = regs[i]; break;
            case TIS_REG_BMS_UNDERVOLTAGE_LIMIT: out->bms_undervoltage_limit = regs[i]; break;
            case TIS_REG_BMS_OVERVOLTAGE_ALARM: out->bms_overvoltage_alarm = regs[i]; break;
            case TIS_REG_BMS_CHARGE_CURRENT_LIMIT: out->bms_charge_current_limit = regs[i]; break;
            case TIS_REG_BMS_DISCHARGE_CURRENT_LIMIT: out->bms_discharge_current_limit = regs[i]; break;
            case TIS_REG_BMS_GROUP_UNDERVOLTAGE_LIMIT: out->bms_group_undervoltage_limit = regs[i]; break;
            case TIS_REG_BMS_GROUP_OVERVOLTAGE_LIMIT: out->bms_group_overvoltage_limit = regs[i]; break;
            case TIS_REG_BMS_GROUP_CHARGE_CURRENT_LIMIT: out->bms_group_charge_current_limit = regs[i]; break;
            case TIS_REG_BMS_GROUP_DISCHARGE_CURRENT_LIMIT: out->bms_group_discharge_current_limit = regs[i]; break;
            case TIS_REG_BATTERY_TYPE: out->battery_type = regs[i]; break;
            case TIS_REG_BATTERY_CAPACITY: out->battery_capacity = regs[i]; break;
            case TIS_REG_TEMPERATURE_COMPENSATION: out->temperature_compensation = regs[i]; break;
            case TIS_REG_TEMPERATURE_UNIT: out->temperature_unit = regs[i]; break;
            case TIS_REG_BACKLIGHT_SECONDS: out->backlight_seconds = regs[i]; break;
            case TIS_REG_DISPLAY_CYCLE_SECONDS: out->display_cycle_seconds = regs[i]; break;
            case TIS_REG_VOLTAGE_LEVEL: out->voltage_level = tis_signed_value(regs[i]); break;
            case TIS_REG_OVERVOLTAGE_DISCONNECT: out->overvoltage_disconnect = regs[i]; break;
            case TIS_REG_CHARGE_VOLTAGE_LIMIT: out->charge_voltage_limit = regs[i]; break;
            case TIS_REG_OVERVOLTAGE_RECONNECT: out->overvoltage_reconnect = regs[i]; break;
            case TIS_REG_EQUALIZE_VOLTAGE: out->equalize_voltage = tis_signed_value(regs[i]); break;
            case TIS_REG_BOOST_VOLTAGE: out->boost_voltage = regs[i]; break;
            case TIS_REG_FLOAT_VOLTAGE: out->float_voltage = regs[i]; break;
            case TIS_REG_BOOST_RECONNECT_VOLTAGE: out->boost_reconnect_voltage = regs[i]; break;
            case TIS_REG_LOW_VOLTAGE_RECONNECT: out->low_voltage_reconnect = tis_signed_value(regs[i]); break;
            case TIS_REG_UNDERVOLTAGE_ALARM_RECONNECT: out->undervoltage_alarm_reconnect = regs[i]; break;
            case TIS_REG_UNDERVOLTAGE_ALARM: out->undervoltage_alarm = regs[i]; break;
            case TIS_REG_LOW_VOLTAGE_DISCONNECT: out->low_voltage_disconnect = regs[i]; break;
            case TIS_REG_DISCHARGE_VOLTAGE_LIMIT: out->discharge_voltage_limit = regs[i]; break;
            case TIS_REG_ALLOWED_CHARGE_CURRENT: out->allowed_charge_current = regs[i]; break;
            case TIS_REG_EQUALIZE_MINUTES: out->equalize_minutes = regs[i]; break;
            case TIS_REG_BOOST_MINUTES: out->boost_minutes = regs[i]; break;
            case TIS_REG_LITHIUM_PROTECTION: out->lithium_protection = regs[i]; break;
            case TIS_REG_LOW_CHARGE_TEMPERATURE: out->low_charge_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_LOW_DISCHARGE_TEMPERATURE: out->low_discharge_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_RTC_MINUTE_SECOND: out->rtc_minute_second = regs[i]; break;
            case TIS_REG_RTC_DAY_HOUR: out->rtc_day_hour = regs[i]; break;
            case TIS_REG_RTC_YEAR_MONTH: out->rtc_year_month = regs[i]; break;
            case TIS_REG_BATTERY_HIGH_TEMPERATURE: out->battery_high_temperature = regs[i]; break;
            case TIS_REG_BATTERY_LOW_TEMPERATURE: out->battery_low_temperature = tis_signed_value(regs[i]); break;
            case TIS_REG_DEVICE_HIGH_TEMPERATURE: out->device_high_temperature = regs[i]; break;
            case TIS_REG_DEVICE_HIGH_TEMPERATURE_RECOVER: out->device_high_temperature_recover = regs[i]; break;
            case TIS_REG_POWER_HIGH_TEMPERATURE: out->power_high_temperature = regs[i]; break;
            case TIS_REG_POWER_HIGH_TEMPERATURE_RECOVER: out->power_high_temperature_recover = regs[i]; break;
            case TIS_REG_LOAD_MODE: out->load_mode = regs[i]; break;
            case TIS_REG_MANUAL_DEFAULT_SWITCH: out->manual_default_switch = regs[i]; break;
            case TIS_REG_LIGHT_ON_VOLTAGE: out->light_on_voltage = regs[i]; break;
            case TIS_REG_LIGHT_ON_DELAY: out->light_on_delay = regs[i]; break;
            case TIS_REG_LIGHT_OFF_VOLTAGE: out->light_off_voltage = regs[i]; break;
            case TIS_REG_LIGHT_OFF_DELAY: out->light_off_delay = regs[i]; break;
            case TIS_REG_NIGHT_DURATION: out->night_duration = regs[i]; break;
            case TIS_REG_SELECTED_PERIOD: out->selected_period = regs[i]; break;
            case TIS_REG_WORK_DURATION1: out->work_duration1 = regs[i]; break;
            case TIS_REG_WORK_DURATION2: out->work_duration2 = regs[i]; break;
            case TIS_REG_ON1_SECOND: out->on1_second = regs[i]; break;
            case TIS_REG_ON1_MINUTE: out->on1_minute = regs[i]; break;
            case TIS_REG_ON1_HOUR: out->on1_hour = regs[i]; break;
            case TIS_REG_OFF1_SECOND: out->off1_second = regs[i]; break;
            case TIS_REG_OFF1_MINUTE: out->off1_minute = regs[i]; break;
            case TIS_REG_OFF1_HOUR: out->off1_hour = regs[i]; break;
            case TIS_REG_ON2_SECOND: out->on2_second = regs[i]; break;
            case TIS_REG_ON2_MINUTE: out->on2_minute = regs[i]; break;
            case TIS_REG_ON2_HOUR: out->on2_hour = regs[i]; break;
            case TIS_REG_OFF2_SECOND: out->off2_second = regs[i]; break;
            case TIS_REG_OFF2_MINUTE: out->off2_minute = regs[i]; break;
            case TIS_REG_OFF2_HOUR: out->off2_hour = regs[i]; break;
            case TIS_REG_CHARGE_DISCHARGE_MODE: out->charge_discharge_mode = regs[i]; break;
            case TIS_REG_FULL_SOC: out->full_soc = regs[i]; break;
            case TIS_REG_FULL_SOC_RECOVER: out->full_soc_recover = regs[i]; break;
            case TIS_REG_DISCHARGE_SOC_RECOVER: out->discharge_soc_recover = regs[i]; break;
            case TIS_REG_LOW_SOC_RECOVER: out->low_soc_recover = regs[i]; break;
            case TIS_REG_LOW_SOC: out->low_soc = regs[i]; break;
            case TIS_REG_DISCHARGE_SOC: out->discharge_soc = regs[i]; break;
            case TIS_REG_RECORD_INTERVAL: out->record_interval = regs[i]; break;
            case TIS_REG_BMS_PROTOCOL_SELECT: out->bms_protocol_select = regs[i]; break;
            case TIS_REG_BMS_PARAMETERS_ENABLED: out->bms_parameters_enabled = regs[i]; break;
            case TIS_REG_PV_PARALLEL_MODE: out->pv_parallel_mode = regs[i]; break;
            case TIS_REG_SIMULATED_BMS: out->simulated_bms = regs[i]; break;
            case TIS_REG_COMMUNICATION_ID: out->communication_id = regs[i]; break;
            case TIS_REG_BAUDRATE: out->baudrate = regs[i]; break;
            case TIS_REG_PARALLEL_CHARGE_CURRENT: out->parallel_charge_current = regs[i]; break;
            case TIS_REG_PV_RESTART_PERIOD: out->pv_restart_period = regs[i]; break;
            case TIS_REG_COMMUNICATION_ENABLED: out->communication_enabled = regs[i]; break;
            case TIS_REG_MANUAL_EQUALIZE: out->manual_equalize = regs[i]; break;
            }
        }
    }
    out->slave_addr = slave_addr;
    return MODBUS_RTU_OK;
}
