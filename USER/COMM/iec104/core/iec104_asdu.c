#include "iec104_asdu.h"

#include <stddef.h>
#include <string.h>

uint16_t iec104_asdu_u16_read(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

uint32_t iec104_asdu_ioa_read(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U);
}

void iec104_asdu_u16_write(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

void iec104_asdu_ioa_write(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
}

uint8_t iec104_asdu_build(uint8_t type, uint8_t vsq, uint8_t cot, uint16_t common_address, const uint8_t *objects, uint8_t objects_length, uint8_t *out, uint8_t out_capacity)
{
    uint8_t asdu_length = IEC104_ASDU_HEADER_LENGTH + objects_length;

    if ((out == NULL) ||
        ((objects_length > 0U) && (objects == NULL)) ||
        (objects_length > (IEC104_MAX_ASDU_LENGTH - IEC104_ASDU_HEADER_LENGTH)) ||
        (asdu_length > out_capacity)) {
        return 0U;
    }

    out[0] = type;
    out[1] = vsq;
    out[2] = cot;
    out[3] = 0U;
    iec104_asdu_u16_write(&out[4], common_address);
    if (objects_length > 0U) {
        memcpy(&out[IEC104_ASDU_HEADER_LENGTH], objects, objects_length);
    }

    return asdu_length;
}
