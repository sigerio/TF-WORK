#include "module_modbus_ac.h"

#include <stddef.h>

typedef enum {
    AC_PARAM_VALUE_I16 = 0,
    AC_PARAM_VALUE_U16,
} ac_param_value_type_t;

typedef struct {
    uint16_t reg;
    uint16_t value_offset;
    uint32_t mark_mask;
    ac_param_value_type_t value_type;
} ac_param_parse_item_t;

typedef struct {
    uint16_t reg;
    uint16_t value_offset;
    uint32_t mark_mask;
} ac_status_parse_item_t;

#define AC_PARAM_ITEM(reg_, field_, mark_, type_) \
    { (reg_), (uint16_t)offsetof(ac_params_t, field_), (mark_), (type_) }
#define AC_STATUS_ITEM(reg_, field_, mark_) \
    { (reg_), (uint16_t)offsetof(ac_status_t, field_), (mark_) }

static const ac_param_parse_item_t s_ac_param_parse_table[] = {
    AC_PARAM_ITEM(AC_PARAM_REG_CABINET_TEMP,           cabinet_temp_deci_c,              (1UL << 0),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_EVAPORATOR_TEMP,        evaporator_temp_deci_c,           (1UL << 1),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_CONDENSER_TEMP,         condenser_temp_deci_c,            (1UL << 2),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY,               humidity_deci_rh,                 (1UL << 3),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_COOLING_START,          cooling_start_deci_c,             (1UL << 4),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_COOLING_STOP,           cooling_stop_deci_c,              (1UL << 5),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HEATING_START,          heating_start_deci_c,             (1UL << 6),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HEATING_STOP,           heating_stop_deci_c,              (1UL << 7),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HIGH_TEMP_ALARM,        high_temp_alarm_deci_c,           (1UL << 8),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_LOW_TEMP_ALARM,         low_temp_alarm_deci_c,            (1UL << 9),  AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_CONDENSER_HIGH_PROTECT, condenser_high_protect_deci_c,    (1UL << 10), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_EVAPORATOR_FREEZE,      evaporator_freeze_protect_deci_c, (1UL << 11), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEHUMIDIFY_START,       dehumidify_start_deci_rh,         (1UL << 12), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEHUMIDIFY_STOP,        dehumidify_stop_deci_rh,          (1UL << 13), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY_ALARM,         humidity_alarm_deci_rh,           (1UL << 14), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_HUMIDITY_CALIBRATION,   humidity_calibration,             (1UL << 15), AC_PARAM_VALUE_I16),
    AC_PARAM_ITEM(AC_PARAM_REG_MODE_CONTROL,           mode_control,                     (1UL << 16), AC_PARAM_VALUE_U16),
    AC_PARAM_ITEM(AC_PARAM_REG_DEVICE_ADDR,            device_addr,                      (1UL << 18), AC_PARAM_VALUE_U16),
};

static const ac_status_parse_item_t s_ac_status_parse_table[] = {
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

static bool ac_ref_to_pdu(uint16_t ref, uint16_t base_ref, uint16_t *out)
{
    if ((out == NULL) || (ref < base_ref)) {
        return false;
    }

    *out = (uint16_t)(ref - base_ref);
    return true;
}

static const ac_param_parse_item_t *module_modbus_ac_find_param_item(uint16_t reg)
{
    for (uint16_t i = 0U; i < (uint16_t)(sizeof(s_ac_param_parse_table) /
                                         sizeof(s_ac_param_parse_table[0])); i++) {
        if (s_ac_param_parse_table[i].reg == reg) {
            return &s_ac_param_parse_table[i];
        }
    }

    return NULL;
}

static const ac_status_parse_item_t *module_modbus_ac_find_status_item(uint16_t reg)
{
    for (uint16_t i = 0U; i < (uint16_t)(sizeof(s_ac_status_parse_table) /
                                         sizeof(s_ac_status_parse_table[0])); i++) {
        if (s_ac_status_parse_table[i].reg == reg) {
            return &s_ac_status_parse_table[i];
        }
    }

    return NULL;
}

static bool module_modbus_ac_param_range_hits_reserved(uint16_t start_ref,
                                                       uint16_t count)
{
    uint32_t end_ref;

    if (count == 0U) {
        return false;
    }

    end_ref = (uint32_t)start_ref + (uint32_t)count - 1U;
    return (((uint32_t)start_ref <= AC_PARAM_REG_RESERVED_30027) &&
            (end_ref >= AC_PARAM_REG_RESERVED_30027));
}

static void module_modbus_ac_apply_param_item(ac_params_t *out,
                                              const ac_param_parse_item_t *item,
                                              uint16_t raw_value)
{
    uint8_t *base = (uint8_t *)out;

    if (item->value_type == AC_PARAM_VALUE_I16) {
        *(int16_t *)(base + item->value_offset) = (int16_t)raw_value;
    } else {
        *(uint16_t *)(base + item->value_offset) = raw_value;
    }

    out->marks.raw |= item->mark_mask;
}

static void module_modbus_ac_apply_status_item(ac_status_t *out,
                                               const ac_status_parse_item_t *item,
                                               bool value)
{
    uint8_t *base = (uint8_t *)out;

    *(bool *)(base + item->value_offset) = value;
    out->marks.raw |= item->mark_mask;
}

bool module_modbus_ac_read_params(uint8_t addr, uint16_t start_ref,
                                  uint16_t count)
{
    uint16_t start_pdu;

    if ((count == 0U) || (count > AC_MAX_READ_REG_COUNT)) {
        return false;
    }
    if (module_modbus_ac_param_range_hits_reserved(start_ref, count)) {
        return false;
    }
    if (!ac_ref_to_pdu(start_ref, AC_PARAM_READ_BASE_REF, &start_pdu)) {
        return false;
    }

    return module_modbus_send_read_regs(addr, start_pdu, count);
}

bool module_modbus_ac_read_status(uint8_t addr, uint16_t start_ref,
                                  uint16_t count)
{
    uint16_t start_pdu;

    if (count == 0U) {
        return false;
    }
    if (!ac_ref_to_pdu(start_ref, AC_STATUS_READ_BASE_REF, &start_pdu)) {
        return false;
    }

    return module_modbus_send_read_bits(addr, start_pdu, count);
}

modbus_rtu_status_t module_modbus_ac_parse_params(const uint8_t *frame,
                                                  uint32_t len, uint8_t addr,
                                                  uint16_t start_ref,
                                                  uint16_t requested_count,
                                                  ac_params_t *out)
{
    uint16_t count = 0U;
    modbus_rtu_status_t status;

    if ((out == NULL) || (requested_count == 0U) ||
        (requested_count > AC_MAX_READ_REG_COUNT)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (module_modbus_ac_param_range_hits_reserved(start_ref, requested_count)) {
        return MODBUS_RTU_ERR_PARAM;
    }

    uint16_t regs[requested_count];

    status = modbus_rtu_parse_read_regs_response(frame,
                                                 len,
                                                 addr,
                                                 MODBUS_FC_READ_HOLDING_REGS,
                                                 regs,
                                                 requested_count,
                                                 &count);
    if (status != MODBUS_RTU_OK) {
        return status;
    }
    if (count != requested_count) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    for (uint16_t i = 0U; i < count; i++) {
        uint16_t reg = (uint16_t)(start_ref + i);
        const ac_param_parse_item_t *item = module_modbus_ac_find_param_item(reg);

        if (item != NULL) {
            module_modbus_ac_apply_param_item(out, item, regs[i]);
        }
    }

    return MODBUS_RTU_OK;
}

modbus_rtu_status_t module_modbus_ac_parse_status(const uint8_t *frame,
                                                  uint32_t len, uint8_t addr,
                                                  uint16_t start_ref,
                                                  ac_status_t *out)
{
    uint8_t bits[66];
    uint16_t bit_count = 0U;
    modbus_rtu_status_t status;

    if (out == NULL) {
        return MODBUS_RTU_ERR_PARAM;
    }

    status = modbus_rtu_parse_read_bits_response(frame,
                                                 len,
                                                 addr,
                                                 MODBUS_FC_READ_COILS,
                                                 bits,
                                                 (uint16_t)(sizeof(bits)),
                                                 &bit_count);
    if (status != MODBUS_RTU_OK) {
        return status;
    }

    for (uint16_t i = 0U; i < bit_count; i++) {
        uint16_t reg = (uint16_t)(start_ref + i);
        const ac_status_parse_item_t *item = module_modbus_ac_find_status_item(reg);
        bool value = (bits[i] != 0U);

        if (item != NULL) {
            module_modbus_ac_apply_status_item(out, item, value);
        }
    }

    return MODBUS_RTU_OK;
}
