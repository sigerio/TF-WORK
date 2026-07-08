#include "module_modbus_device_common.h"

modbus_rtu_status_t module_modbus_parse_u16_regs(const uint8_t *frame,
                                                 uint32_t len,
                                                 uint8_t addr,
                                                 uint16_t *regs,
                                                 uint16_t reg_capacity,
                                                 uint16_t *reg_count)
{
    return modbus_rtu_parse_read_regs_response(frame,
                                               len,
                                               addr,
                                               MODBUS_FC_READ_HOLDING_REGS,
                                               regs,
                                               reg_capacity,
                                               reg_count);
}

static int32_t module_modbus_reg_to_i32(uint16_t value,
                                        module_modbus_reg_value_type_t type)
{
    if (type == MODULE_MODBUS_REG_VALUE_I16) {
        return (int32_t)((int16_t)value);
    }

    return (int32_t)value;
}

static void module_modbus_store_range_value(
    uint8_t *base,
    const module_modbus_reg_range_item_t *item,
    uint16_t value)
{
    if (item->value_type == MODULE_MODBUS_REG_VALUE_I16) {
        *(int16_t *)(base + item->value_offset) = (int16_t)value;
    } else {
        *(uint16_t *)(base + item->value_offset) = value;
    }
}

modbus_rtu_status_t module_modbus_parse_regs_with_ranges(
    const uint8_t *frame,
    uint32_t len,
    uint8_t addr,
    uint16_t expected_count,
    void *out,
    uint16_t status_offset,
    const module_modbus_reg_range_item_t *items,
    uint16_t item_count)
{
    uint16_t regs[MODBUS_RTU_MAX_REG_COUNT];
    uint16_t count = 0U;
    uint8_t *base = (uint8_t *)out;
    module_modbus_device_status_t *status;
    modbus_rtu_status_t parse_status;

    if ((out == NULL) || (expected_count == 0U) ||
        (expected_count > MODBUS_RTU_MAX_REG_COUNT) ||
        ((items == NULL) && (item_count != 0U))) {
        return MODBUS_RTU_ERR_PARAM;
    }

    for (uint16_t i = 0U; i < item_count; i++) {
        if (items[i].reg_index >= expected_count) {
            return MODBUS_RTU_ERR_PARAM;
        }
    }

    parse_status = module_modbus_parse_u16_regs(frame,
                                                len,
                                                addr,
                                                regs,
                                                expected_count,
                                                &count);
    if (parse_status != MODBUS_RTU_OK) {
        return parse_status;
    }
    if (count != expected_count) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    status = (module_modbus_device_status_t *)(base + status_offset);
    status->online = true;
    status->sensor_fault = false;

    for (uint16_t i = 0U; i < item_count; i++) {
        const module_modbus_reg_range_item_t *item = &items[i];
        int32_t value;

        value = module_modbus_reg_to_i32(regs[item->reg_index], item->value_type);
        if ((value < item->min_value) || (value > item->max_value)) {
            status->sensor_fault = true;
        } else {
            module_modbus_store_range_value(base, item, regs[item->reg_index]);
        }
    }

    return MODBUS_RTU_OK;
}
