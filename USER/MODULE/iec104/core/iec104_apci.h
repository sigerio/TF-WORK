#ifndef IEC104_APCI_H
#define IEC104_APCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IEC104_START_BYTE       0x68U
#define IEC104_CONTROL_LENGTH   4U
#define IEC104_MAX_ASDU_LENGTH  249U
#define IEC104_MAX_FRAME_LENGTH 255U

typedef enum {
    IEC104_FRAME_I = 0,
    IEC104_FRAME_S,
    IEC104_FRAME_U,
} iec104_frame_format_e;

typedef enum {
    IEC104_U_NONE = 0x00U,
    IEC104_U_STARTDT_ACT = 0x07U,
    IEC104_U_STARTDT_CON = 0x0BU,
    IEC104_U_STOPDT_ACT = 0x13U,
    IEC104_U_STOPDT_CON = 0x23U,
    IEC104_U_TESTFR_ACT = 0x43U,
    IEC104_U_TESTFR_CON = 0x83U,
} iec104_u_command_e;

typedef struct {
    iec104_frame_format_e format;
    uint16_t send_sequence;
    uint16_t receive_sequence;
    iec104_u_command_e u_command;
    uint8_t asdu_length;
    uint8_t asdu[IEC104_MAX_ASDU_LENGTH];
} iec104_frame_t;

uint16_t iec104_build_i(uint8_t *apdu, uint16_t apdu_capacity, const uint8_t *asdu, uint8_t asdu_length, uint16_t send_sequence, uint16_t receive_sequence);
uint16_t iec104_build_s(uint8_t *apdu, uint16_t apdu_capacity, uint16_t receive_sequence);
uint16_t iec104_build_u(uint8_t *apdu, uint16_t apdu_capacity, iec104_u_command_e u_command);
uint8_t iec104_decode(const uint8_t *apdu, uint16_t apdu_length, iec104_frame_t *decoded);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_APCI_H */
