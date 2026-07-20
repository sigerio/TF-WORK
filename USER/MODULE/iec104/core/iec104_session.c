#include "iec104_session.h"

#include <stddef.h>
#include <string.h>

#include "iec104_txq.h"

#define IEC104_SEQUENCE_LIMIT 0x8000U
#define IEC104_SEQUENCE_MASK  (IEC104_SEQUENCE_LIMIT - 1U)

#define IEC104_T1_MS 15000U
#define IEC104_T2_MS 10000U
#define IEC104_T3_MS 20000U
#define IEC104_K     12U
#define IEC104_W     8U

typedef struct {
    uint16_t next_sequence;
    uint16_t first_unacknowledged_sequence;
    uint8_t unacknowledged_count;
} iec104_send_window_t;

typedef struct {
    uint16_t expected_sequence;
    uint8_t pending_count;
    uint32_t first_pending_tick;
} iec104_receive_ack_t;

typedef struct {
    uint8_t data_transfer_started;
    uint8_t test_frame_pending;
    uint32_t last_activity_tick;
    uint32_t test_frame_tick;
} iec104_link_supervision_t;

typedef struct {
    iec104_send_window_t send_window;
    iec104_receive_ack_t receive_ack;
    iec104_link_supervision_t link;
    iec104_session_callbacks_t callbacks;
} iec104_session_state_t;

static iec104_session_state_t session;

static uint8_t iec104_received_acknowledges(uint16_t receive_sequence)
{
    uint16_t distance;

    distance = (uint16_t)((receive_sequence + IEC104_SEQUENCE_LIMIT - session.send_window.first_unacknowledged_sequence) & IEC104_SEQUENCE_MASK); //等价于对IEC104_SEQUENCE_LIMIT取余运算
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
    if ((session.link.data_transfer_started == 0U) ||
        (session.send_window.unacknowledged_count >= IEC104_K)) {
        return 0U;
    }

    if (!iec104_send_i(asdu, asdu_length, session.send_window.next_sequence, session.receive_ack.expected_sequence)) {
        return 0U;
    }

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
    iec104_activity_record(now);
    return 1U;
}

uint8_t iec104_session_data_transfer_started(void)
{
    return session.link.data_transfer_started;
}

static void iec104_handle_i_frame(const iec104_frame_t *frame, uint32_t now)
{
    if (!iec104_received_acknowledges(frame->receive_sequence) ||
        (frame->send_sequence != session.receive_ack.expected_sequence)) {
        return;
    }

    session.receive_ack.expected_sequence = (uint16_t)((session.receive_ack.expected_sequence + 1U) & IEC104_SEQUENCE_MASK);
    if (session.receive_ack.pending_count == 0U) {
        session.receive_ack.first_pending_tick = now;
    }
    session.receive_ack.pending_count++;

    if ((session.link.data_transfer_started != 0U) &&
        (session.callbacks.on_asdu != NULL)) {
        session.callbacks.on_asdu(frame, now);
    }

    if (session.receive_ack.pending_count >= IEC104_W) {
        (void)iec104_send_s_ack(now);
    }
}

static void iec104_discard_queued_i_frames(void)
{
    uint8_t removed_count = iec104_txq_remove_i_frames();

    session.send_window.next_sequence =
        (uint16_t)((session.send_window.next_sequence +
                    IEC104_SEQUENCE_LIMIT - removed_count) &
                   IEC104_SEQUENCE_MASK);
    if (removed_count <= session.send_window.unacknowledged_count) {
        session.send_window.unacknowledged_count -= removed_count;
    } else {
        session.send_window.unacknowledged_count = 0U;
    }
    if (session.send_window.unacknowledged_count == 0U) {
        session.send_window.first_unacknowledged_sequence =
            session.send_window.next_sequence;
    }
}

static void iec104_handle_u_frame(const iec104_frame_t *frame, uint32_t now)
{
    switch (frame->u_command) {
    case IEC104_U_STARTDT_ACT:
        session.link.data_transfer_started = 1U;
        (void)iec104_send_u(IEC104_U_STARTDT_CON);
        break;

    case IEC104_U_STOPDT_ACT:
        session.link.data_transfer_started = 0U;
        if (session.callbacks.on_data_transfer_stop != NULL) {
            session.callbacks.on_data_transfer_stop();
        }
        iec104_discard_queued_i_frames();
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
    if ((session.receive_ack.pending_count > 0U) &&
        ((now - session.receive_ack.first_pending_tick) >= IEC104_T2_MS)) {
        (void)iec104_send_s_ack(now);
    }

    if ((session.link.test_frame_pending != 0U) &&
        ((now - session.link.test_frame_tick) >= IEC104_T1_MS)) {
        session.link.test_frame_pending = 0U;
        iec104_activity_record(now);
    }

    if ((session.link.test_frame_pending == 0U) &&
        ((now - session.link.last_activity_tick) >= IEC104_T3_MS) &&
        iec104_send_u(IEC104_U_TESTFR_ACT)) {
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
    session.link.last_activity_tick = now;
    session.receive_ack.first_pending_tick = now;
    session.link.test_frame_tick = now;
    iec104_txq_init();
}

