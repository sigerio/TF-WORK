#include "COMM/modbus/devices/modbus_wanrong.h"

uint32_t modbus_wanrong_build_read_status(uint8_t slave_addr, uint8_t *frame, uint32_t capacity)
{
    if (slave_addr == 0U || slave_addr > 247U) return 0U;
    return modbus_rtu_build_read_request(slave_addr, MODBUS_FC_READ_HOLDING_REGS,
        WANRONG_REG_INPUT_STATUS, 1U, frame, capacity);
}

modbus_rtu_status_t modbus_wanrong_parse_status(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint16_t *inputs)
{
    uint16_t value;
    uint16_t count;
    modbus_rtu_status_t status;

    if (inputs == 0 || slave_addr == 0U || slave_addr > 247U) return MODBUS_RTU_ERR_PARAM;
    status = modbus_rtu_parse_read_regs_response(frame, len, slave_addr,
        MODBUS_FC_READ_HOLDING_REGS, &value, 1U, &count);
    if (status != MODBUS_RTU_OK) return status;
    if (count != 1U) return MODBUS_RTU_ERR_BYTE_COUNT;
    *inputs = value;
    return MODBUS_RTU_OK;
}

uint32_t modbus_wanrong_build_command(uint8_t slave_addr, wanrong_command_e command, uint8_t *frame, uint32_t capacity)
{
    if ((slave_addr == 0U) || (slave_addr > 247U) || ((unsigned)command > WANRONG_REVERSE)) {
        return 0U;
    }
    return modbus_rtu_build_write_single_request(slave_addr, MODBUS_FC_WRITE_SINGLE_REG,
        WANRONG_REG_COMMAND, (uint16_t)command, frame, capacity);
}

modbus_rtu_status_t modbus_wanrong_parse_ack(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint16_t command)
{
    if ((slave_addr == 0U) || (slave_addr > 247U) || (command > WANRONG_REVERSE)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    return modbus_rtu_parse_write_single_response(frame, len, slave_addr, MODBUS_FC_WRITE_SINGLE_REG, WANRONG_REG_COMMAND, command);
}
