#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include <stdint.h>

uint16_t modbus_rtu_crc16(const uint8_t *data, uint32_t len);
void modbus_rtu_append_crc(uint8_t *frame, uint32_t len_without_crc);

#endif
