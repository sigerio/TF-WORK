#ifndef IEC104_APCI_H
#define IEC104_APCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * APCI 编解码原语。
 *
 * APDU 固定以 0x68 开始，随后是 1 字节长度域和 4 字节控制域。I 帧在控制域后
 * 携带 ASDU，S/U 帧不携带 ASDU。
 */
#define IEC104_START_BYTE       0x68U
#define IEC104_CONTROL_LENGTH   4U
#define IEC104_MAX_ASDU_LENGTH  249U
#define IEC104_MAX_FRAME_LENGTH 255U

/* APCI 控制域决定的三种帧格式。 */
typedef enum {
    IEC104_FRAME_I = 0,
    IEC104_FRAME_S,
    IEC104_FRAME_U,
} iec104_frame_format_e;

/* U 帧控制命令，枚举值即控制域首字节的编码值。 */
typedef enum {
    IEC104_U_NONE = 0x00U,
    IEC104_U_STARTDT_ACT = 0x07U,
    IEC104_U_STARTDT_CON = 0x0BU,
    IEC104_U_STOPDT_ACT = 0x13U,
    IEC104_U_STOPDT_CON = 0x23U,
    IEC104_U_TESTFR_ACT = 0x43U,
    IEC104_U_TESTFR_CON = 0x83U,
} iec104_u_command_e;

/* 解码后的统一帧表示；仅 I 帧的 asdu_length/asdu 有效。 */
typedef struct {
    iec104_frame_format_e format;
    uint16_t send_sequence;
    uint16_t receive_sequence;
    iec104_u_command_e u_command;
    uint8_t asdu_length;
    uint8_t asdu[IEC104_MAX_ASDU_LENGTH];
} iec104_frame_t;

/* 编码成功返回 APDU 长度，参数或容量非法返回 0。 */
uint16_t iec104_build_i(uint8_t *apdu, uint16_t apdu_capacity, const uint8_t *asdu, uint8_t asdu_length, uint16_t send_sequence, uint16_t receive_sequence);
uint16_t iec104_build_s(uint8_t *apdu, uint16_t apdu_capacity, uint16_t receive_sequence);
uint16_t iec104_build_u(uint8_t *apdu, uint16_t apdu_capacity, iec104_u_command_e u_command);

/* 校验完整 APDU 并解码 I/S/U 控制域，成功返回 1。 */
uint8_t iec104_decode(const uint8_t *apdu, uint16_t apdu_length, iec104_frame_t *decoded);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_APCI_H */
