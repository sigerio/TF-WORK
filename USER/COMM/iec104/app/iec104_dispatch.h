#ifndef IEC104_DISPATCH_H
#define IEC104_DISPATCH_H

#include "../core/iec104_apci.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ASDU 命令分发（应用层）。
 *
 * 把已通过 APCI 会话确认的 I 帧按类型标识解释为工程语义：单点遥控、总召唤、
 * 设定命令（遥调），以及变位遥信的主动上送。分发层依赖 core 的 ASDU 编解码
 * 原语与 iec104_app 的工程点表/执行回调，向上经 iec104_session_send_asdu 出帧。
 */

void iec104_dispatch_init(void);

/* STOPDT 时清理选择/暂存状态（作为会话回调）。 */
void iec104_dispatch_reset(void);

/* 处理一帧确认后的 ASDU（作为会话回调）。 */
void iec104_dispatch_handle(const iec104_frame_t *frame, uint32_t now);

/* 变位遥信主动上送，由慢周期轮询任务调用。 */
void iec104_dispatch_send_spontaneous(uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_DISPATCH_H */
