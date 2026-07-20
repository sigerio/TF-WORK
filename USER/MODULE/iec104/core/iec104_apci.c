#include "iec104_apci.h"

#include <string.h>

static uint8_t iec104_u_command_is_valid(iec104_u_command_e u_command)
{
    switch (u_command) {
    case IEC104_U_STARTDT_ACT:
    case IEC104_U_STARTDT_CON:
    case IEC104_U_STOPDT_ACT:
    case IEC104_U_STOPDT_CON:
    case IEC104_U_TESTFR_ACT:
    case IEC104_U_TESTFR_CON:
        return 1U;

    default:
        return 0U;
    }
}

static uint16_t iec104_sequence_decode(const uint8_t *control)
{
    uint16_t sequence = (uint16_t)control[0] |
                        ((uint16_t)control[1] << 8U);

    return sequence >> 1U;
}

static void iec104_sequence_encode(uint8_t *control, uint16_t sequence)
{
    uint16_t encoded_sequence = (uint16_t)(sequence << 1U);

    control[0] = (uint8_t)encoded_sequence;
    control[1] = (uint8_t)(encoded_sequence >> 8U);
}

uint16_t iec104_build_i(uint8_t *apdu, uint16_t apdu_capacity, const uint8_t *asdu, uint8_t asdu_length, uint16_t send_sequence, uint16_t receive_sequence)
{
    uint16_t apdu_length = IEC104_CONTROL_LENGTH + asdu_length + 2U;

    if ((apdu == NULL) || (asdu == NULL) || (asdu_length == 0U) ||
        (asdu_length > IEC104_MAX_ASDU_LENGTH) ||
        (send_sequence > 0x7FFFU) || (receive_sequence > 0x7FFFU) ||
        (apdu_length > apdu_capacity)) {
        return 0U;
    }

    apdu[0] = IEC104_START_BYTE;
    apdu[1] = IEC104_CONTROL_LENGTH + asdu_length;
    iec104_sequence_encode(&apdu[2], send_sequence);
    iec104_sequence_encode(&apdu[4], receive_sequence);
    memcpy(&apdu[6], asdu, asdu_length);

    return apdu_length;
}

uint16_t iec104_build_s(uint8_t *apdu, uint16_t apdu_capacity, uint16_t receive_sequence)
{
    if ((apdu == NULL) || (receive_sequence > 0x7FFFU) ||
        (apdu_capacity < 6U)) {
        return 0U;
    }

    apdu[0] = IEC104_START_BYTE;
    apdu[1] = IEC104_CONTROL_LENGTH;
    apdu[2] = 0x01U;
    apdu[3] = 0x00U;
    iec104_sequence_encode(&apdu[4], receive_sequence);

    return 6U;
}

uint16_t iec104_build_u(uint8_t *apdu, uint16_t apdu_capacity, iec104_u_command_e u_command)
{
    if ((apdu == NULL) || (apdu_capacity < 6U) ||
        !iec104_u_command_is_valid(u_command)) {
        return 0U;
    }

    apdu[0] = IEC104_START_BYTE;
    apdu[1] = IEC104_CONTROL_LENGTH;
    apdu[2] = (uint8_t)u_command;
    apdu[3] = 0x00U;
    apdu[4] = 0x00U;
    apdu[5] = 0x00U;

    return 6U;
}
/*帧类型解码*/
uint8_t iec104_decode(const uint8_t *apdu, uint16_t apdu_length, iec104_frame_t *decoded)
{
    const uint8_t *control;
    uint8_t body_length;
    uint8_t asdu_length;

    if ((apdu == NULL) || (decoded == NULL) || (apdu_length < 6U) ||
        (apdu[0] != IEC104_START_BYTE)) {
        return 0U;
    }

    body_length = apdu[1];
    if ((body_length < IEC104_CONTROL_LENGTH) ||
        (body_length > (IEC104_MAX_FRAME_LENGTH - 2U)) ||
        (apdu_length != (uint16_t)body_length + 2U)) {
        return 0U;
    }

    control = &apdu[2];
    asdu_length = body_length - IEC104_CONTROL_LENGTH;
    if (asdu_length > IEC104_MAX_ASDU_LENGTH) {
        return 0U;
    }

    if (((control[0] & 0x01U) == 0U) && ((control[2] & 0x01U) == 0U) && (asdu_length > 0U))
    {
        decoded->format = IEC104_FRAME_I;
        decoded->send_sequence = iec104_sequence_decode(&control[0]);
        decoded->receive_sequence = iec104_sequence_decode(&control[2]);
        decoded->u_command = IEC104_U_NONE;
    }
    else if ((control[0] == 0x01U) && (control[1] == 0U) && ((control[2] & 0x01U) == 0U) &&
            (control[3] == 0U) && (asdu_length == 0U))
    {
        decoded->format = IEC104_FRAME_S;
        decoded->send_sequence = 0U;
        decoded->receive_sequence = iec104_sequence_decode(&control[2]);
        decoded->u_command = IEC104_U_NONE;
    }
    else if (((control[0] & 0x03U) == 0x03U) && (control[1] == 0U) && (control[2] == 0U) &&
            (control[3] == 0U) && (asdu_length == 0U) && iec104_u_command_is_valid((iec104_u_command_e)control[0]))
    {
        decoded->format = IEC104_FRAME_U;
        decoded->send_sequence = 0U;
        decoded->receive_sequence = 0U;
        decoded->u_command = (iec104_u_command_e)control[0];
    }
    else
    {
        return 0U;
    }

    decoded->asdu_length = asdu_length;
    if (asdu_length > 0U) {
        memcpy(decoded->asdu, &apdu[6], asdu_length);
    }

    return 1U;
}
