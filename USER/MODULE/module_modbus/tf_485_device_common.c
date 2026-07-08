#include "tf_485_device_common.h"

modbus_rtu_status_t tf_485_parse_u16_regs(const uint8_t *frame,
                                                 uint32_t len,
                                                 uint8_t slave_addr,
                                                 uint16_t *regs,
                                                 uint16_t reg_capacity,
                                                 uint16_t *reg_count)
{
    return modbus_rtu_parse_read_regs_response(frame,
                                               len,
                                               slave_addr,
                                               MODBUS_FC_READ_HOLDING_REGS,
                                               regs,
                                               reg_capacity,
                                               reg_count);
}

static int32_t tf_485_reg_to_i32(uint16_t value,
                                        tf_485_reg_value_t type)
{
    if (type == TF_485_REG_I16) {
        return (int32_t)((int16_t)value);
    }

    return (int32_t)value;
}

static void tf_485_store_range_value(
    uint8_t *base,
    const tf_485_reg_map_t *map,
    uint16_t value)
{
    if (map->reg_type == TF_485_REG_I16) {
        *(int16_t *)(base + map->field_offset) = (int16_t)value;
    } else {
        *(uint16_t *)(base + map->field_offset) = value;
    }
}

modbus_rtu_status_t tf_485_parse_ranges(
    const uint8_t *frame,
    uint32_t len,
    uint8_t slave_addr,
    uint16_t expected_regs,
    void *out,
    uint16_t status_offset,
    const tf_485_reg_map_t *maps,
    uint16_t map_count)
{
    uint16_t regs[MODBUS_RTU_MAX_REG_COUNT];
    uint16_t parsed_regs = 0U;
    uint8_t *base = (uint8_t *)out;
    tf_485_dev_status_t *status;
    modbus_rtu_status_t parse_status;

    if ((out == NULL) || (expected_regs == 0U) ||
        (expected_regs > MODBUS_RTU_MAX_REG_COUNT) ||
        ((maps == NULL) && (map_count != 0U))) {
        return MODBUS_RTU_ERR_PARAM;
    }

    for (uint16_t i = 0U; i < map_count; i++) {
        if (maps[i].reg_offset >= expected_regs) {
            return MODBUS_RTU_ERR_PARAM;
        }
    }

    parse_status = tf_485_parse_u16_regs(frame,
                                                len,
                                                slave_addr,
                                                regs,
                                                expected_regs,
                                                &parsed_regs);
    if (parse_status != MODBUS_RTU_OK) {
        return parse_status;
    }
    if (parsed_regs != expected_regs) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    status = (tf_485_dev_status_t *)(base + status_offset);
    status->online = true;
    status->sensor_fault = false;

    for (uint16_t i = 0U; i < map_count; i++) {
        const tf_485_reg_map_t *map = &maps[i];
        int32_t value;

        value = tf_485_reg_to_i32(regs[map->reg_offset], map->reg_type);
        if ((value < map->min_value) || (value > map->max_value)) {
            status->sensor_fault = true;
        } else {
            tf_485_store_range_value(base, map, regs[map->reg_offset]);
        }
    }

    return MODBUS_RTU_OK;
}
