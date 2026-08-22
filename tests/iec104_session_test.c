#include <assert.h>
#include <stdint.h>

#include "iec104_apci.h"
#include "iec104_session.h"
#include "iec104_txq.h"

static uint8_t stop_callback_count;

static void stop_callback(void)
{
    stop_callback_count++;
}

static void handle_u_frame(iec104_u_command_e command, uint32_t now)
{
    iec104_frame_t frame = {0};

    frame.format = IEC104_FRAME_U;
    frame.u_command = command;
    iec104_session_handle_frame(&frame, now);
}

int main(void)
{
    static const iec104_session_callbacks_t callbacks = {
        .on_asdu = 0,
        .on_data_transfer_stop = stop_callback,
    };
    uint8_t asdu[] = {0x64U};
    uint8_t apdu[IEC104_MAX_FRAME_LENGTH];
    uint16_t apdu_length;

    iec104_session_init(&callbacks, 0U);
    handle_u_frame(IEC104_U_STARTDT_ACT, 1U);
    assert(iec104_session_data_transfer_started());
    assert(iec104_session_send_asdu(asdu, sizeof(asdu), 2U));

    handle_u_frame(IEC104_U_STOPDT_ACT, 3U);
    assert(!iec104_session_data_transfer_started());
    assert(stop_callback_count == 1U);
    assert(iec104_txq_pop(apdu, sizeof(apdu), &apdu_length));
    assert(apdu_length == 6U);
    assert(apdu[2] == IEC104_U_STOPDT_CON);
    assert(!iec104_txq_pop(apdu, sizeof(apdu), &apdu_length));

    handle_u_frame(IEC104_U_STARTDT_ACT, 4U);
    assert(iec104_session_send_asdu(asdu, sizeof(asdu), 5U));
    assert(iec104_txq_pop(apdu, sizeof(apdu), &apdu_length));
    assert(apdu[2] == IEC104_U_STARTDT_CON);
    assert(iec104_txq_pop(apdu, sizeof(apdu), &apdu_length));
    assert(apdu[2] == 0U);
    assert(apdu[3] == 0U);
    return 0;
}
