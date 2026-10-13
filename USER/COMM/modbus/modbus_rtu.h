#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stdint.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_RTU_MIN_FRAME_LEN        4U
#define MODBUS_RTU_REQUEST_LEN          8U
#define MODBUS_RTU_EXCEPTION_LEN        5U
#define MODBUS_RTU_WRITE_SINGLE_RESPONSE_LEN 8U
#define MODBUS_RTU_MAX_REG_COUNT        125U
#define MODBUS_RTU_MAX_BIT_COUNT        2000U

#define MODBUS_FC_READ_COILS            0x01U
#define MODBUS_FC_READ_HOLDING_REGS     0x03U
#define MODBUS_FC_READ_INPUT_REGS       0x04U
#define MODBUS_FC_WRITE_SINGLE_COIL     0x05U
#define MODBUS_FC_WRITE_SINGLE_REG      0x06U
#define MODBUS_FC_WRITE_MULTIPLE_REGS   0x10U

typedef enum {
    MODBUS_RTU_OK = 0,
    MODBUS_RTU_ERR_PARAM,
    MODBUS_RTU_ERR_LEN,
    MODBUS_RTU_ERR_CRC,
    MODBUS_RTU_ERR_ADDR,
    MODBUS_RTU_ERR_FUNC,
    MODBUS_RTU_ERR_EXCEPTION,
    MODBUS_RTU_ERR_BYTE_COUNT,
    MODBUS_RTU_ERR_VALUE,
} modbus_rtu_status_t;

typedef struct {
    uint8_t address;        /* 返回异常响应的从机地址。 */
    uint8_t function;       /* 原请求功能码，已去除异常标志位。 */
    uint8_t exception_code; /* Modbus 异常码。 */
} modbus_rtu_exception_t;

/* CRC、字节序和请求构帧原语，不管理串口或请求队列。 */
uint16_t modbus_rtu_crc16(const uint8_t *data, uint32_t len);
bool modbus_rtu_check_crc(const uint8_t *frame, uint32_t len);
void modbus_rtu_append_crc(uint8_t *frame, uint32_t len_without_crc);
uint16_t modbus_rtu_get_u16_be(const uint8_t *data);
void modbus_rtu_put_u16_be(uint8_t *data, uint16_t value);

uint32_t modbus_rtu_build_read_request(uint8_t addr, uint8_t function, uint16_t start_reg, uint16_t count, uint8_t *out, uint32_t out_size);
uint32_t modbus_rtu_build_write_single_request(uint8_t addr, uint8_t function,
    uint16_t reg, uint16_t value, uint8_t *out, uint32_t out_size);
uint32_t modbus_rtu_build_write_regs_request(uint8_t addr, uint16_t start_reg,
    const uint16_t *values, uint16_t count, uint8_t *out, uint32_t out_size);
modbus_rtu_status_t modbus_rtu_parse_write_regs_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint16_t start_reg, uint16_t count);

/* 校验异常响应；匹配时将从机异常写入 exception。 */
modbus_rtu_status_t modbus_rtu_parse_exception(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, modbus_rtu_exception_t *exception);
/* 校验读寄存器或读位响应，并解析为调用方提供的数组。 */
modbus_rtu_status_t modbus_rtu_parse_read_regs_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint16_t *regs, uint16_t reg_capacity, uint16_t *reg_count);
modbus_rtu_status_t modbus_rtu_parse_read_bits_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint8_t *bits, uint16_t bit_capacity, uint16_t *bit_count);
modbus_rtu_status_t modbus_rtu_parse_write_single_response(const uint8_t *frame, uint32_t len,
    uint8_t expect_addr, uint8_t expect_function, uint16_t expect_reg, uint16_t expect_value);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */
