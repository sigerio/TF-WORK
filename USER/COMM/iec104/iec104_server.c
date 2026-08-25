#include "iec104_server.h"

#include <stddef.h>
#include <stdio.h>

#include "../../OS/tf_os.h"
#include "../../tf_debug.h"
#include "app/iec104_dispatch.h"
#include "core/iec104_apci.h"
#include "core/iec104_session.h"
#include "core/iec104_txq.h"
#include "port/iec104_port.h"

/*
 * IEC104 服务器门面。
 *
 * 只负责装配各层并编排物理链路与协议内核之间的字节搬运：
 *   - 从 port 收字节 → 组帧解码 → 交给 session 处理
 *   - 从 txq 取待发 APDU → 经 port 发送
 *   - 由通信任务驱动 session 协议定时器，由报告任务驱动主动上送组帧
 * 具体协议状态机在 core/iec104_session，命令语义在 app/iec104_dispatch。
 */

static uint8_t receive_apdu[IEC104_MAX_FRAME_LENGTH];

static void iec104_log_frame(const char *direction,
                             const uint8_t *frame,
                             uint16_t length)
{
    if(0 == debug_ctrl.bits.iec)    return;
    
    char line[128];
    uint16_t offset = 0U;

    if ((direction == NULL) || (frame == NULL) || (length == 0U)) {
        return;
    }

    /* 分段打印，避免长 APDU 超过调试串口格式化缓冲区。 */
    while (offset < length) {
        int used = snprintf(line, sizeof(line),
                            "IEC104 %s len=%u:", direction,
                            (unsigned)length);
        uint16_t count = 0U;

        if (used < 0) {
            return;
        }
        while ((count < 24U) && (offset < length) &&
               ((uint32_t)used + 4U < sizeof(line))) {
            used += snprintf(&line[used], sizeof(line) - (size_t)used,
                             " %02X", frame[offset++]);
            count++;
        }
        (void)snprintf(&line[used], sizeof(line) - (size_t)used, "\n");
        TF_DEBUG("APP", "%s", line);
    }
}

static uint8_t iec104_server_receive_frame(iec104_frame_t *frame)
{
    uint8_t header[2];
    uint16_t scan_limit;
    uint16_t scan_index;
    uint16_t apdu_length;

    if (frame == NULL) {
        return 0U;
    }

    scan_limit = iec104_port_rx_available();
    /* 丢弃无效前导字节，直到 0x68 和合法长度域重新建立帧同步。 */
    for (scan_index = 0U; scan_index < scan_limit; scan_index++) {
        if (!iec104_port_rx_peek(header, sizeof(header))) {
            return 0U;
        }

        if ((header[0] != IEC104_START_BYTE) ||
            (header[1] < IEC104_CONTROL_LENGTH) ||
            (header[1] > (IEC104_MAX_FRAME_LENGTH - 2U))) {
            (void)iec104_port_rx_read(header, 1U);
            continue;
        }

        apdu_length = (uint16_t)header[1] + 2U;
        /* read 只在完整 APDU 已进入 FIFO 时消费数据，半帧留待下个周期。 */
        if (!iec104_port_rx_read(receive_apdu, apdu_length)) {
            return 0U;
        }

        iec104_log_frame("RX", receive_apdu, apdu_length);

        if (iec104_decode(receive_apdu, apdu_length, frame)) {
            return 1U;
        }
    }

    return 0U;
}

static void iec104_receive_available_frames(uint32_t now)
{
    iec104_frame_t frame;

    /* 本轮持续取帧，直到 FIFO 为空或队头 APDU 尚未收全。 */
    while (iec104_server_receive_frame(&frame) != 0U) {
        iec104_session_handle_frame(&frame, now);
    }
}

static void iec104_send_queued_apdu(void)
{
    uint8_t *apdu;
    uint16_t apdu_length;

    if (iec104_port_tx_busy()) {
        return;
    }

    apdu = iec104_port_tx_buffer();
    if (apdu == NULL) {
        return;
    }

    if (!iec104_txq_pop(apdu, IEC104_MAX_FRAME_LENGTH, &apdu_length)) {
        return;
    }

    iec104_log_frame("TX", apdu, apdu_length);
    (void)iec104_port_tx_start(apdu_length);
}

void iec104_server_init(void)
{
    static const iec104_session_callbacks_t callbacks = {
        .on_asdu = iec104_dispatch_handle,
        .on_data_transfer_stop = iec104_dispatch_reset,
    };

    iec104_session_init(&callbacks, tf_os_tick_get());
    iec104_dispatch_init();
}

void iec104_server_comm_task(void)
{
    uint32_t now = tf_os_tick_get();

    iec104_port_tx_service();
    if (iec104_port_rx_available() > 0U) {
        iec104_receive_available_frames(now);
    }
    iec104_session_service_timers(now);
    iec104_send_queued_apdu();
}

void iec104_server_report_task(void)
{
    uint32_t now = tf_os_tick_get();

    if (iec104_session_data_transfer_started() != 0U) {
        iec104_dispatch_send_spontaneous(now);
    }
}
