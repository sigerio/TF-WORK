#ifndef IEC104_SESSION_H
#define IEC104_SESSION_H

#include "COMM/iec104/core/iec104_apci.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * APCI 会话状态机（协议内核层）。
 *
 * 负责发送/接收序号、K/W 窗口确认、T1/T2/T3 超时监视与 U 帧握手，
 * 并通过发送队列（iec104_txq）产出待发 APDU。会话层不直接触碰物理链路，
 * 也不含任何工程点表：它把已解码的帧交给上层处理，把待发帧压入队列，
 * 由门面层（iec104_server）负责与 port 之间的字节搬运。
 *
 * 与应用分发层（iec104_dispatch）的耦合全部经由回调完成，使本层可脱离
 * 具体工程复用与单元测试。
 */

typedef struct {
    /* 收到带数据的 I 帧且链路处于数据传输态时调用。 */
    void (*on_asdu)(const iec104_frame_t *frame, uint32_t now);
    /* 串口会话收到 STOPDT 后清理应用侧选择/暂存状态时调用。 */
    void (*on_data_transfer_stop)(void);
} iec104_session_callbacks_t;

void iec104_session_init(const iec104_session_callbacks_t *callbacks, uint32_t now);

/* 处理一帧已解码的 APDU（I/S/U）。 */
void iec104_session_handle_frame(const iec104_frame_t *frame, uint32_t now);

/* 周期维护 T1/T2/T3 超时与延迟确认，应由通信任务定期调用。 */
void iec104_session_service_timers(uint32_t now);

/* 链路是否处于数据传输态（STARTDT 之后）。 */
uint8_t iec104_session_data_transfer_started(void);

/*
 * 应用分发层的上行入口：把一段 ASDU 作为 I 帧压入发送队列。
 * 窗口已满或未处于数据传输态时返回 0（上层可无损重试）。
 */
uint8_t iec104_session_send_asdu(const uint8_t *asdu, uint8_t asdu_length, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_SESSION_H */
