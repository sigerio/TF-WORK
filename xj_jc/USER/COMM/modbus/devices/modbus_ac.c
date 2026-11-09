#include "COMM/modbus/devices/modbus_ac.h"

#include <stddef.h>

#include "COMM/modbus/modbus_config.h"

typedef enum {
    AC_REG_I16 = 0,
    AC_REG_U16,
} ac_reg_value_t;

typedef struct {
    uint16_t reg;
    uint16_t field_offset;
    uint32_t mark_mask;
    ac_reg_value_t reg_type;
} ac_param_map_t;

typedef struct {
    uint16_t reg;
    uint16_t field_offset;
    uint32_t mark_mask;
} ac_status_map_t;

#define AC_PARAM_ITEM(reg_, field_, mark_, type_) \
    { (reg_), (uint16_t)offsetof(ac_params_t, field_), (mark_), (type_) }
#define AC_STATUS_ITEM(reg_, field_, mark_) \
    { (reg_), (uint16_t)offsetof(ac_status_t, field_), (mark_) }

static const ac_param_map_t ac_parameter_maps[] = {
    AC_PARAM_ITEM(AC_PARAM_REG_CABINET_TEMP,           cabinet_temp_deci_c,              (1UL << 0),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_EVAPORATOR_TEMP,        evaporator_temp_deci_c,           (1UL << 1),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_CONDENSER_TEMP,         condenser_temp_deci_c,            (1UL << 2),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY,               humidity_deci_rh,                 (1UL << 3),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_COOLING_START,          cooling_start_deci_c,             (1UL << 4),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_COOLING_STOP,           cooling_stop_deci_c,              (1UL << 5),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HEATING_START,          heating_start_deci_c,             (1UL << 6),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HEATING_STOP,           heating_stop_deci_c,              (1UL << 7),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HIGH_TEMP_ALARM,        high_temp_alarm_deci_c,           (1UL << 8),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_LOW_TEMP_ALARM,         low_temp_alarm_deci_c,            (1UL << 9),  AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_CONDENSER_HIGH_PROTECT, condenser_high_protect_deci_c,    (1UL << 10), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_EVAPORATOR_FREEZE,      evaporator_freeze_protect_deci_c, (1UL << 11), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEHUMIDIFY_START,       dehumidify_start_deci_rh,         (1UL << 12), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEHUMIDIFY_STOP,        dehumidify_stop_deci_rh,          (1UL << 13), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY_ALARM,         humidity_alarm_deci_rh,           (1UL << 14), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY_CALIBRATION,   humidity_calibration,             (1UL << 15), AC_REG_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_MODE_CONTROL,           mode_control,                     (1UL << 16), AC_REG_U16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEVICE_ADDR,            device_addr,                      (1UL << 18), AC_REG_U16),
};

static const ac_status_map_t ac_status_maps[] = {
    AC_STATUS_ITEM(AC_STATUS_COOLING_ON,               cooling_on,                       (1UL << 0)),
    AC_STATUS_ITEM(AC_STATUS_HEATER_ON,                heater_on,                        (1UL << 1)),
    AC_STATUS_ITEM(AC_STATUS_INNER_FAN_ON,             inner_fan_on,                     (1UL << 2)),
    AC_STATUS_ITEM(AC_STATUS_OUTER_FAN_ON,             outer_fan_on,                     (1UL << 3)),
    AC_STATUS_ITEM(AC_STATUS_POWER_MONITOR,            monitor_shutdown,                 (1UL << 4)),
    AC_STATUS_ITEM(AC_STATUS_CABINET_SENSOR_FAULT,     cabinet_sensor_fault,             (1UL << 5)),
    AC_STATUS_ITEM(AC_STATUS_EVAPORATOR_SENSOR_FAULT,  evaporator_sensor_fault,          (1UL << 6)),
    AC_STATUS_ITEM(AC_STATUS_CONDENSER_SENSOR_FAULT,   condenser_sensor_fault,           (1UL << 7)),
    AC_STATUS_ITEM(AC_STATUS_HUMIDITY_SENSOR_FAULT,    humidity_sensor_fault,            (1UL << 8)),
    AC_STATUS_ITEM(AC_STATUS_HUMIDITY_HIGH_ALARM,      humidity_high_alarm,              (1UL << 9)),
    AC_STATUS_ITEM(AC_STATUS_CONDENSER_PRESSURE_ALARM, condenser_high_pressure_alarm,    (1UL << 10)),
    AC_STATUS_ITEM(AC_STATUS_EVAPORATOR_FREEZE_ALARM,  evaporator_freeze_alarm,          (1UL << 11)),
    AC_STATUS_ITEM(AC_STATUS_COOLING_ALARM,            cooling_alarm,                    (1UL << 12)),
    AC_STATUS_ITEM(AC_STATUS_HIGH_TEMP_ALARM,          high_temp_alarm,                  (1UL << 13)),
    AC_STATUS_ITEM(AC_STATUS_LOW_TEMP_ALARM,           low_temp_alarm,                   (1UL << 14)),
};

static const ac_param_map_t *modbus_ac_find_param(uint16_t reg)
{
    for (uint16_t i = 0U; i < (uint16_t)(sizeof(ac_parameter_maps) / sizeof(ac_parameter_maps[0])); i++) {
        if (ac_parameter_maps[i].reg == reg) {
            return &ac_parameter_maps[i];
        }
    }

    return NULL;
}

static const ac_status_map_t *modbus_ac_find_status(uint16_t reg)
{
    for (uint16_t i = 0U; i < (uint16_t)(sizeof(ac_status_maps) / sizeof(ac_status_maps[0])); i++) {
        if (ac_status_maps[i].reg == reg) {
            return &ac_status_maps[i];
        }
    }

    return NULL;
}

static bool modbus_ac_has_reserved_param(uint16_t start_ref, uint16_t count)
{
    uint32_t end_ref;

    if (count == 0U) {
        return false;
    }

    end_ref = (uint32_t)start_ref + (uint32_t)count - 1U;
    return (((uint32_t)start_ref <= AC_PARAM_REG_RESERVED_30027) && (end_ref >= AC_PARAM_REG_RESERVED_30027));
}

static void modbus_ac_apply_param(ac_params_t *out, const ac_param_map_t *param_map, uint16_t raw_value)
{
    uint8_t *base = (uint8_t *)out;

    if (param_map->reg_type == AC_REG_I16) {
        *(int16_t *)(base + param_map->field_offset) = (int16_t)raw_value;
    } else {
        *(uint16_t *)(base + param_map->field_offset) = raw_value;
    }

    out->marks.raw |= param_map->mark_mask;
}

static void modbus_ac_apply_status(ac_status_t *out, const ac_status_map_t *status_map, bool value)
{
    uint8_t *base = (uint8_t *)out;

    *(bool *)(base + status_map->field_offset) = value;
    out->marks.raw |= status_map->mark_mask;
}

static uint32_t modbus_ac_build_read_request(uint8_t slave_addr, bool read_bits, uint16_t start_reg, uint16_t reg_count,
    uint8_t *frame, uint32_t capacity)
{
    uint16_t base = read_bits ? AC_STATUS_READ_BASE_REF : AC_PARAM_READ_BASE_REF;
    uint32_t response_bytes = read_bits ? ((uint32_t)reg_count + 7U) / 8U : (uint32_t)reg_count * 2U;
    uint8_t function = read_bits ? MODBUS_FC_READ_COILS : MODBUS_FC_READ_HOLDING_REGS;

    if ((slave_addr == 0U) || (reg_count == 0U) || (start_reg < base)) {
        return 0U;
    }
    if ((response_bytes == 0U) || (response_bytes > 0xFFU) || ((response_bytes + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
        return 0U;
    }

    return modbus_rtu_build_read_request(slave_addr, function, (uint16_t)(start_reg - base), reg_count, frame, capacity);
}

uint32_t modbus_ac_build_read_params_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity)
{
    if ((reg_count == 0U) || (reg_count > AC_MAX_READ_REG_COUNT)) {
        return 0U;
    }
    if (modbus_ac_has_reserved_param(start_reg, reg_count)) {
        return 0U;
    }
    if (start_reg < AC_PARAM_READ_BASE_REF) {
        return 0U;
    }

    return modbus_ac_build_read_request(slave_addr, false, start_reg, reg_count, frame, capacity);
}

uint32_t modbus_ac_build_read_status_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *frame, uint32_t capacity)
{
    if (reg_count == 0U) {
        return 0U;
    }
    if (start_reg < AC_STATUS_READ_BASE_REF) {
        return 0U;
    }

    return modbus_ac_build_read_request(slave_addr, true, start_reg, reg_count, frame, capacity);
}

uint32_t modbus_ac_build_write_status_request(uint8_t slave_addr, uint16_t status_ref, bool on, uint8_t *frame, uint32_t capacity)
{
    uint16_t pdu_ref;
    uint16_t value;

    if (slave_addr == 0U) {
        return 0U;
    }
    if (status_ref != AC_STATUS_POWER_MONITOR) {
        return 0U;
    }

    pdu_ref = (uint16_t)(status_ref - AC_STATUS_READ_BASE_REF);
    value = on ? 0xFF00U : 0x0000U;

    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_COIL, pdu_ref, value, frame, capacity);
}

uint32_t modbus_ac_build_write_param_request(uint8_t slave_addr, uint16_t param_ref, int16_t value, uint8_t *frame, uint32_t capacity)
{
    uint16_t pdu_ref;

    if (slave_addr == 0U) {
        return 0U;
    }
    if ((param_ref == AC_PARAM_REG_RESERVED_30027) || (param_ref == AC_PARAM_REG_DEVICE_ADDR)) {
        return 0U;
    }
    if ((param_ref < AC_PARAM_REG_COOLING_START) || (param_ref > AC_PARAM_REG_MODE_CONTROL) || (modbus_ac_find_param(param_ref) == NULL)) {
        return 0U;
    }

    pdu_ref = (uint16_t)(param_ref - AC_PARAM_READ_BASE_REF);
    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG, pdu_ref, (uint16_t)value, frame, capacity);
}

modbus_rtu_status_t modbus_ac_parse_params(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint16_t start_ref, uint16_t requested_count, ac_params_t *out)
{
    uint16_t count = 0U;
    modbus_rtu_status_t status;

    if ((out == NULL) || (requested_count == 0U) || (requested_count > AC_MAX_READ_REG_COUNT)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (modbus_ac_has_reserved_param(start_ref, requested_count)) {
        return MODBUS_RTU_ERR_PARAM;
    }

    uint16_t regs[requested_count];

    status = modbus_rtu_parse_read_regs_response(frame, len, slave_addr, MODBUS_FC_READ_HOLDING_REGS, regs, requested_count, &count);
    if (status != MODBUS_RTU_OK) {
        return status;
    }
    if (count != requested_count) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    for (uint16_t i = 0U; i < count; i++) {
        uint16_t reg = (uint16_t)(start_ref + i);
        const ac_param_map_t *param_map = modbus_ac_find_param(reg);

        if (param_map != NULL) {
            modbus_ac_apply_param(out, param_map, regs[i]);
        }
    }

    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_ac_parse_write_ack(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint8_t function, uint16_t ref, uint16_t value, ac_device_t *out)
{
    modbus_rtu_status_t status;

    status = modbus_rtu_parse_write_single_response(frame, len, slave_addr, function, ref, value);
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    if (out != NULL) {
        out->slave_addr = slave_addr;
        out->status.online = true;
    }

    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_ac_parse_status(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint16_t start_ref, ac_status_t *out)
{
    uint8_t bits[66];
    uint16_t bit_count = 0U;
    modbus_rtu_status_t status;

    if (out == NULL) {
        return MODBUS_RTU_ERR_PARAM;
    }

    status = modbus_rtu_parse_read_bits_response(frame, len, slave_addr, MODBUS_FC_READ_COILS, bits, (uint16_t)(sizeof(bits)), &bit_count);
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    for (uint16_t i = 0U; i < bit_count; i++) {
        uint16_t reg = (uint16_t)(start_ref + i);
        const ac_status_map_t *status_map = modbus_ac_find_status(reg);
        bool value = (bits[i] != 0U);

        if (status_map != NULL) {
            modbus_ac_apply_status(out, status_map, value);
        }
    }

    return MODBUS_RTU_OK;
}
