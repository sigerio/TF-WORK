#include "COMM/modbus/modbus_rtu.h"

uint16_t modbus_rtu_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;

    if (data == 0) {
        return 0U;
    }

    for (uint32_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

bool modbus_rtu_check_crc(const uint8_t *frame, uint32_t len)
{
    uint16_t crc;

    if ((frame == 0) || (len < MODBUS_RTU_MIN_FRAME_LEN)) {
        return false;
    }

    crc = modbus_rtu_crc16(frame, len - 2U);
    return ((uint8_t)(crc & 0xFFU) == frame[len - 2U]) && ((uint8_t)(crc >> 8U) == frame[len - 1U]);
}

void modbus_rtu_append_crc(uint8_t *frame, uint32_t len_without_crc)
{
    uint16_t crc;

    if (frame == 0) {
        return;
    }

    crc = modbus_rtu_crc16(frame, len_without_crc);
    frame[len_without_crc] = (uint8_t)(crc & 0xFFU);
    frame[len_without_crc + 1U] = (uint8_t)(crc >> 8U);
}

uint16_t modbus_rtu_get_u16_be(const uint8_t *data)
{
    if (data == 0) {
        return 0U;
    }

    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

void modbus_rtu_put_u16_be(uint8_t *data, uint16_t value)
{
    if (data == 0) {
        return;
    }

    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)(value & 0xFFU);
}

uint32_t modbus_rtu_build_read_request(uint8_t addr, uint8_t function, uint16_t start_reg, uint16_t count, uint8_t *out, uint32_t out_size)
{
    if ((out == 0) || (out_size < MODBUS_RTU_REQUEST_LEN) || (count == 0U)) {
        return 0U;
    }

    switch (function) {
    case MODBUS_FC_READ_COILS:
        if (count > MODBUS_RTU_MAX_BIT_COUNT) {
            return 0U;
        }
        break;
    case MODBUS_FC_READ_HOLDING_REGS:
        if (count > MODBUS_RTU_MAX_REG_COUNT) {
            return 0U;
        }
        break;
    default:
        return 0U;
    }

    if (((uint32_t)start_reg + (uint32_t)count) > 0x10000UL) {
        return 0U;
    }

    out[0] = addr;
    out[1] = function;
    modbus_rtu_put_u16_be(&out[2], start_reg);
    modbus_rtu_put_u16_be(&out[4], count);
    modbus_rtu_append_crc(out, 6U);
    return MODBUS_RTU_REQUEST_LEN;
}

uint32_t modbus_rtu_build_write_single_request(uint8_t addr, uint8_t function,
    uint16_t reg, uint16_t value, uint8_t *out, uint32_t out_size)
{
    if ((out == 0) || (out_size < MODBUS_RTU_REQUEST_LEN)) {
        return 0U;
    }

    switch (function) {
    case MODBUS_FC_WRITE_SINGLE_COIL:
        if ((value != 0x0000U) && (value != 0xFF00U)) {
            return 0U;
        }
        break;

    case MODBUS_FC_WRITE_SINGLE_REG:
        break;

    default:
        return 0U;
    }

    out[0] = addr;
    out[1] = function;
    modbus_rtu_put_u16_be(&out[2], reg);
    modbus_rtu_put_u16_be(&out[4], value);
    modbus_rtu_append_crc(out, 6U);
    return MODBUS_RTU_REQUEST_LEN;
}

modbus_rtu_status_t modbus_rtu_parse_exception(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, modbus_rtu_exception_t *exception)
{
    if ((frame == 0) || (exception == 0)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (len != MODBUS_RTU_EXCEPTION_LEN) {
        return MODBUS_RTU_ERR_LEN;
    }
    if (!modbus_rtu_check_crc(frame, len)) {
        return MODBUS_RTU_ERR_CRC;
    }
    if (frame[0] != expect_addr) {
        return MODBUS_RTU_ERR_ADDR;
    }
    if (frame[1] != (uint8_t)(expect_function | 0x80U)) {
        return MODBUS_RTU_ERR_FUNC;
    }

    exception->address = frame[0];
    exception->function = expect_function;
    exception->exception_code = frame[2];
    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_rtu_parse_read_regs_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint16_t *regs, uint16_t reg_capacity, uint16_t *reg_count)
{
    uint8_t byte_count;
    uint16_t count;

    if ((frame == 0) || (regs == 0) || (reg_count == 0)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (len < 5U) {
        return MODBUS_RTU_ERR_LEN;
    }
    if (!modbus_rtu_check_crc(frame, len)) {
        return MODBUS_RTU_ERR_CRC;
    }
    if (frame[0] != expect_addr) {
        return MODBUS_RTU_ERR_ADDR;
    }
    if (frame[1] != expect_function) {
        return ((frame[1] & 0x80U) != 0U) ? MODBUS_RTU_ERR_EXCEPTION : MODBUS_RTU_ERR_FUNC;
    }

    byte_count = frame[2];
    if (((byte_count & 0x01U) != 0U) || (len != (uint32_t)byte_count + 5U)) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    count = (uint16_t)(byte_count / 2U);
    if (count > reg_capacity) {
        return MODBUS_RTU_ERR_PARAM;
    }

    for (uint16_t i = 0U; i < count; i++) {
        regs[i] = modbus_rtu_get_u16_be(&frame[3U + ((uint32_t)i * 2U)]);
    }

    *reg_count = count;
    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_rtu_parse_read_bits_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint8_t *bits, uint16_t bit_capacity, uint16_t *bit_count)
{
    uint8_t byte_count;
    uint16_t out_count = 0U;

    if ((frame == 0) || (bits == 0) || (bit_count == 0)) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (len < 5U) {
        return MODBUS_RTU_ERR_LEN;
    }
    if (!modbus_rtu_check_crc(frame, len)) {
        return MODBUS_RTU_ERR_CRC;
    }
    if (frame[0] != expect_addr) {
        return MODBUS_RTU_ERR_ADDR;
    }
    if (frame[1] != expect_function) {
        return ((frame[1] & 0x80U) != 0U) ? MODBUS_RTU_ERR_EXCEPTION : MODBUS_RTU_ERR_FUNC;
    }

    byte_count = frame[2];
    if (len != (uint32_t)byte_count + 5U) {
        return MODBUS_RTU_ERR_BYTE_COUNT;
    }

    for (uint8_t byte = 0U; byte < byte_count; byte++) {
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if (out_count >= bit_capacity) {
                *bit_count = out_count;
                return MODBUS_RTU_OK;
            }
            bits[out_count++] = (uint8_t)((frame[3U + byte] >> bit) & 0x01U);
        }
    }

    *bit_count = out_count;
    return MODBUS_RTU_OK;
}

modbus_rtu_status_t modbus_rtu_parse_write_single_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint16_t expect_reg, uint16_t expect_value)
{
    uint16_t reg;
    uint16_t value;

    if (frame == 0) {
        return MODBUS_RTU_ERR_PARAM;
    }
    if (len != MODBUS_RTU_WRITE_SINGLE_RESPONSE_LEN) {
        return MODBUS_RTU_ERR_LEN;
    }
    if (!modbus_rtu_check_crc(frame, len)) {
        return MODBUS_RTU_ERR_CRC;
    }
    if (frame[0] != expect_addr) {
        return MODBUS_RTU_ERR_ADDR;
    }
    if (frame[1] != expect_function) {
        return ((frame[1] & 0x80U) != 0U) ? MODBUS_RTU_ERR_EXCEPTION : MODBUS_RTU_ERR_FUNC;
    }
    if ((expect_function != MODBUS_FC_WRITE_SINGLE_COIL) && (expect_function != MODBUS_FC_WRITE_SINGLE_REG)) {
        return MODBUS_RTU_ERR_FUNC;
    }

    reg = modbus_rtu_get_u16_be(&frame[2]);
    value = modbus_rtu_get_u16_be(&frame[4]);
    if ((reg != expect_reg) || (value != expect_value)) {
        return MODBUS_RTU_ERR_VALUE;
    }

    return MODBUS_RTU_OK;
}
