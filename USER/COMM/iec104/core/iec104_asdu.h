#ifndef IEC104_ASDU_H
#define IEC104_ASDU_H

#include "iec104_apci.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ASDU 编解码原语（协议内核层）。
 *
 * 本文件只提供 IEC 60870-5-101/104 标准定义的类型标识、传送原因常量，
 * 以及信息体字节序读写与 ASDU 组帧工具。它不含任何工程点表或命令语义，
 * 因此可被不同工程的应用分发层（iec104_dispatch）直接复用。
 */

/* 类型标识 TI */
#define IEC104_TI_M_SP_NA_1 0x01U /* 单点遥信 */
#define IEC104_TI_M_ME_NC_1 0x0DU /* 短浮点遥测 */
#define IEC104_TI_C_SC_NA_1 0x2DU /* 单点遥控 */
#define IEC104_TI_C_IC_NA_1 0x64U /* 总召唤 */
#define IEC104_TI_C_WS_NA_1 0xCAU /* 设定命令（遥调） */

/* 传送原因 COT */
#define IEC104_COT_ACT          0x06U
#define IEC104_COT_ACTCON       0x07U
#define IEC104_COT_ACTTERM      0x0AU
#define IEC104_COT_SPONT        0x03U
#define IEC104_COT_INTROGEN     0x14U
#define IEC104_COT_UNKNOWN_TYPE 0x2CU
#define IEC104_COT_UNKNOWN_COT  0x2DU
#define IEC104_COT_UNKNOWN_CA   0x2EU
#define IEC104_COT_UNKNOWN_IOA  0x2FU
#define IEC104_COT_NEGATIVE     0x40U

#define IEC104_ASDU_HEADER_LENGTH 6U

/* 读取/写入 3 字节无符号 IOA；IOA 按低字节在前编码。 */
uint32_t iec104_asdu_ioa_read(const uint8_t *data);
void iec104_asdu_ioa_write(uint8_t *data, uint32_t value);

/* 读取/写入 2 字节无符号字段（IEC104 采用小端）。 */
uint16_t iec104_asdu_u16_read(const uint8_t *data);
void iec104_asdu_u16_write(uint8_t *data, uint16_t value);

/*
 * 组装完整 ASDU（6 字节数据单元标识符 + 信息体）到 out。
 * 返回 ASDU 总长度；参数非法或容量不足返回 0。
 */
uint8_t iec104_asdu_build(uint8_t type, uint8_t vsq, uint8_t cot, uint16_t common_address, const uint8_t *objects, uint8_t objects_length, uint8_t *out, uint8_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_ASDU_H */
