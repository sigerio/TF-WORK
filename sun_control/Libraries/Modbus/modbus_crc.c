#include "modbus_crc.h"

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
