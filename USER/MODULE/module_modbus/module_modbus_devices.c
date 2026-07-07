#include "module_modbus_devices.h"

uint32_t module_modbus_build_read_regs(uint8_t addr, uint16_t start_reg,
                                       uint16_t count, uint8_t *out,
                                       uint32_t out_size)
{
    return modbus_rtu_build_read_request(addr,
                                         MODBUS_FC_READ_HOLDING_REGS,
                                         start_reg,
                                         count,
                                         out,
                                         out_size);
}

uint32_t module_modbus_build_read_bits(uint8_t addr, uint16_t start_bit,
                                       uint16_t count, uint8_t *out,
                                       uint32_t out_size)
{
    return modbus_rtu_build_read_request(addr,
                                         MODBUS_FC_READ_COILS,
                                         start_bit,
                                         count,
                                         out,
                                         out_size);
}

bool module_modbus_send_read_regs(uint8_t addr, uint16_t start_reg,
                                  uint16_t count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = module_modbus_build_read_regs(addr, start_reg, count,
                                                 frame, sizeof(frame));
    return (len > 0U) ? module_modbus_send(frame, len) : false;
}

bool module_modbus_send_read_bits(uint8_t addr, uint16_t start_bit,
                                  uint16_t count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    uint32_t len = module_modbus_build_read_bits(addr, start_bit, count,
                                                frame, sizeof(frame));
    return (len > 0U) ? module_modbus_send(frame, len) : false;
}

bool module_modbus_xunchip_read_measurements(uint8_t addr,
                                             module_modbus_device_type_t type)
{
    uint16_t count;

    switch (type) {
    case MODULE_MODBUS_DEV_XM7828_TH:
        count = XM7828_MEASUREMENT_REG_COUNT;
        break;

    case MODULE_MODBUS_DEV_XM8189_WIND:
        count = XM8189_MEASUREMENT_REG_COUNT;
        break;

    case MODULE_MODBUS_DEV_XM8537_WATER:
        count = XM8537_MEASUREMENT_REG_COUNT;
        break;

    case MODULE_MODBUS_DEV_XM9505_RAIN:
        count = XM9505_MEASUREMENT_REG_COUNT;
        break;

    default:
        return false;
    }

    return module_modbus_send_read_regs(addr, XUNCHIP_REG_DATA0, count);
}

bool module_modbus_xunchip_read_device_addr(uint8_t addr)
{
    return module_modbus_send_read_regs(addr, XUNCHIP_REG_DEVICE_ADDR, 1U);
}

modbus_rtu_status_t module_modbus_parse_u16_regs(const uint8_t *frame,
                                                 uint32_t len, uint8_t addr,
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

static void module_modbus_store_range_value(uint8_t *base,
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
    uint16_t marks_offset,
    uint16_t faults_offset,
    const module_modbus_reg_range_item_t *items,
    uint16_t item_count)
{
    uint16_t regs[MODBUS_RTU_MAX_REG_COUNT];
    uint16_t count = 0U;
    uint8_t *base = (uint8_t *)out;
    module_modbus_device_status_t *status;
    uint32_t *marks;
    uint32_t *faults;
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
    marks = (uint32_t *)(base + marks_offset);
    faults = (uint32_t *)(base + faults_offset);

    status->raw = 0U;
    *marks = 0U;
    *faults = 0U;
    status->bit.online = 1U;

    for (uint16_t i = 0U; i < item_count; i++) {
        const module_modbus_reg_range_item_t *item = &items[i];
        int32_t value;

        value = module_modbus_reg_to_i32(regs[item->reg_index], item->value_type);
        if ((value < item->min_value) || (value > item->max_value)) {
            *faults |= item->fault_mask;
        } else {
            module_modbus_store_range_value(base, item, regs[item->reg_index]);
            *marks |= item->mark_mask;
        }
    }

    if (*faults != 0U) {
        status->bit.data_fault = 1U;
    }

    return MODBUS_RTU_OK;
}
