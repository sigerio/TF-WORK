#include "COMM/iec104/core/iec104_session.h"

#include <stddef.h>
#include <string.h>

#include "COMM/iec104/core/iec104_txq.h"

#define IEC104_SEQUENCE_LIMIT 0x8000U
#define IEC104_SEQUENCE_MASK  (IEC104_SEQUENCE_LIMIT - 1U)

#define IEC104_T1_MS 15000U
#define IEC104_T2_MS 10000U
#define IEC104_T3_MS 20000U
#define IEC104_K     12U
#define IEC104_W     8U

/* 本端 I 帧发送窗口：下一个 N(S)、最早未确认 N(S) 和窗口占用数。 */
typedef struct {
    uint16_t next_sequence;
    uint16_t first_unacknowledged_sequence;
    uint8_t unacknowledged_count;
} iec104_send_window_t;

/* 对端 I 帧接收确认：期望 N(S) 以及尚未通过 N(R) 确认的帧数。 */
typedef struct {
    uint16_t expected_sequence;
    uint8_t pending_count;
} iec104_receive_ack_t;

/* STARTDT 数据传输状态、周期 S 帧和 T1/T3 测试帧监视状态。 */
typedef struct {
    uint8_t data_transfer_started;
    uint8_t test_frame_pending;
    uint32_t last_activity_tick;
    uint32_t last_s_frame_tick;
    uint32_t test_frame_tick;
} iec104_link_supervision_t;

typedef struct {
    iec104_send_window_t send_window;
    iec104_receive_ack_t receive_ack;
    iec104_link_supervision_t link;
    iec104_session_callbacks_t callbacks;
} iec104_session_state_t;

/* 保留全局符号，便于调试器 Watch 直接查看 APCI 会话窗口和定时状态。 */
iec104_session_state_t session;

static uint8_t iec104_received_acknowledges(uint16_t receive_sequence)
{
    uint16_t distance;

    /*
     * 序列号按 32768 回绕。N(R) 与最早未确认序号的距离不能超过当前发送窗口，
     * 否则对端确认了本端尚未发送的 I 帧。
     */
    distance = (uint16_t)((receive_sequence + IEC104_SEQUENCE_LIMIT - session.send_window.first_unacknowledged_sequence) & IEC104_SEQUENCE_MASK);
    if (distance > session.send_window.unacknowledged_count) {
        return 0U;
    }

    session.send_window.first_unacknowledged_sequence = receive_sequence;
    session.send_window.unacknowledged_count -= (uint8_t)distance;
    return 1U;
}

static void iec104_activity_record(uint32_t now)
{
    session.link.last_activity_tick = now;
}

static void iec104_session_reset(uint32_t now)
{
    iec104_session_callbacks_t callbacks = session.callbacks;

    memset(&session, 0, sizeof(session));
    session.callbacks = callbacks;
    session.link.last_activity_tick = now;
    session.link.last_s_frame_tick = now;
    session.link.test_frame_tick = now;
    iec104_txq_init();
}

static uint8_t iec104_send_i(const uint8_t *asdu, uint8_t asdu_length, uint16_t send_sequence, uint16_t receive_sequence)
{
    uint8_t apdu[IEC104_MAX_FRAME_LENGTH];
    uint16_t apdu_length;

    apdu_length = iec104_build_i(apdu, sizeof(apdu), asdu, asdu_length, send_sequence, receive_sequence);
    return (apdu_length > 0U) && iec104_txq_push(apdu, apdu_length);
}

static uint8_t iec104_send_s(uint16_t receive_sequence)
{
    uint8_t apdu[6];
    uint16_t apdu_length;

    apdu_length = iec104_build_s(apdu, sizeof(apdu), receive_sequence);
    return (apdu_length > 0U) && iec104_txq_push(apdu, apdu_length);
}

static uint8_t iec104_send_u(iec104_u_command_e u_command)
{
    uint8_t apdu[6];
    uint16_t apdu_length;

    apdu_length = iec104_build_u(apdu, sizeof(apdu), u_command);
    return (apdu_length > 0U) && iec104_txq_push(apdu, apdu_length);
}

uint8_t iec104_session_send_asdu(const uint8_t *asdu, uint8_t asdu_length, uint32_t now)
{
    if ((session.link.data_transfer_started == 0U) || (session.send_window.unacknowledged_count >= IEC104_K)) {
        return 0U;
    }

    if (!iec104_send_i(asdu, asdu_length, session.send_window.next_sequence, session.receive_ack.expected_sequence)) {
        return 0U;
    }

    /* 只有 APDU 成功入队后才占用发送窗口，避免入队失败造成 N(S) 断层。 */
    if (session.send_window.unacknowledged_count == 0U) {
        session.send_window.first_unacknowledged_sequence = session.send_window.next_sequence;
    }
    session.send_window.next_sequence = (uint16_t)((session.send_window.next_sequence + 1U) & IEC104_SEQUENCE_MASK);
    session.send_window.unacknowledged_count++;
    session.receive_ack.pending_count = 0U;
    iec104_activity_record(now);
    return 1U;
}

static uint8_t iec104_send_s_ack(uint32_t now)
{
    if (!iec104_send_s(session.receive_ack.expected_sequence)) {
        return 0U;
    }

    session.receive_ack.pending_count = 0U;
    session.link.last_s_frame_tick = now;
    iec104_activity_record(now);
    return 1U;
}

uint8_t iec104_session_data_transfer_started(void)
{
    return session.link.data_transfer_started;
}

static void iec104_handle_i_frame(const iec104_frame_t *frame, uint32_t now)
{
    /* I 帧同时确认本端发送窗口；对端 N(S) 必须与当前期望值严格连续。 */
    if (!iec104_received_acknowledges(frame->receive_sequence) || (frame->send_sequence != session.receive_ack.expected_sequence)) {
        return;
    }

    session.receive_ack.expected_sequence = (uint16_t)((session.receive_ack.expected_sequence + 1U) & IEC104_SEQUENCE_MASK);
    session.receive_ack.pending_count++;

    if ((session.link.data_transfer_started != 0U) && (session.callbacks.on_asdu != NULL)) {
        session.callbacks.on_asdu(frame, now);
    }

    /* 收满 W 帧立即发 S 帧；未满 W 时由 T2 定时器延迟确认。 */
    if (session.receive_ack.pending_count >= IEC104_W) {
        (void)iec104_send_s_ack(now);
    }
}

static void iec104_handle_u_frame(const iec104_frame_t *frame, uint32_t now)
{
    switch (frame->u_command) {
    case IEC104_U_STARTDT_ACT:
        session.link.data_transfer_started = 1U;
        session.link.last_s_frame_tick = now;
        (void)iec104_send_u(IEC104_U_STARTDT_CON);
        break;

    case IEC104_U_STOPDT_ACT:
        if (session.callbacks.on_data_transfer_stop != NULL) {
            session.callbacks.on_data_transfer_stop();
        }
        iec104_session_reset(now);
        (void)iec104_send_u(IEC104_U_STOPDT_CON);
        break;

    case IEC104_U_TESTFR_ACT:
        (void)iec104_send_u(IEC104_U_TESTFR_CON);
        break;

    case IEC104_U_TESTFR_CON:
        session.link.test_frame_pending = 0U;
        break;

    default:
        break;
    }

    iec104_activity_record(now);
}

void iec104_session_handle_frame(const iec104_frame_t *frame, uint32_t now)
{
    if (frame == NULL) {
        return;
    }

    switch (frame->format) {
    case IEC104_FRAME_I:
        iec104_handle_i_frame(frame, now);
        break;

    case IEC104_FRAME_S:
        (void)iec104_received_acknowledges(frame->receive_sequence);
        break;

    case IEC104_FRAME_U:
        iec104_handle_u_frame(frame, now);
        break;

    default:
        break;
    }

    iec104_activity_record(now);
}

void iec104_session_service_timers(uint32_t now)
{
    /* T2：数据传输期间定期发送 S 帧，上报最新 N(R)。 */
    if ((session.link.data_transfer_started != 0U) && ((now - session.link.last_s_frame_tick) >= IEC104_T2_MS)) {
        (void)iec104_send_s_ack(now);
    }

    /* T1：测试确认超时后结束本轮等待，允许后续 T3 再次发起链路测试。 */
    if ((session.link.test_frame_pending != 0U) && ((now - session.link.test_frame_tick) >= IEC104_T1_MS)) {
        session.link.test_frame_pending = 0U;
        iec104_activity_record(now);
    }

    /* T3：链路空闲且没有待确认测试帧时发送 TESTFR ACT。 */
    if ((session.link.test_frame_pending == 0U) &&
        ((now - session.link.last_activity_tick) >= IEC104_T3_MS) && iec104_send_u(IEC104_U_TESTFR_ACT)) {
        session.link.test_frame_pending = 1U;
        session.link.test_frame_tick = now;
        iec104_activity_record(now);
    }
}

void iec104_session_init(const iec104_session_callbacks_t *callbacks, uint32_t now)
{
    memset(&session, 0, sizeof(session));
    if (callbacks != NULL) {
        session.callbacks = *callbacks;
    }
    iec104_session_reset(now);
}
