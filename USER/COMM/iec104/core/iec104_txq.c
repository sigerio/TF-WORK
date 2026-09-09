#include "COMM/iec104/core/iec104_txq.h"

#include <string.h>

#include "COMM/iec104/core/iec104_apci.h"

/*
 * 发送队列 —— 长度前缀式线性字节缓冲区。
 *
 * 队列不再限制帧数，只受总字节数约束：每帧以 2 字节小端长度前缀 + APDU 负载
 * 连续存放于 buf[head, tail) 区间内。数据始终保持线性（不跨越缓冲区尾部回绕）：
 * 当尾部剩余空间不足以容纳新帧、但整体空闲足够时，先将已排队数据整体前移压缩，
 * 再写入新帧。这样出队与丢弃 I 帧都退化为简单的线性操作，无需处理环形回绕。
 *
 * 满缓冲策略：拒绝新帧（push 返回 0）。上层 iec104_server 的序号仅在 push 成功后
 * 才递增、遥信变位仅在发送成功后才提交，因此入队失败天然无损重试，序号不产生断层。
 */

#define IEC104_TXQ_BUFFER_SIZE 2048U
#define IEC104_TXQ_PREFIX_SIZE 2U

static uint8_t txq_buf[IEC104_TXQ_BUFFER_SIZE];
static uint16_t txq_head; /* 读偏移：下一帧起始位置 */
static uint16_t txq_tail; /* 写偏移：末字节的下一个位置 */

static uint16_t iec104_txq_used(void)
{
    return (uint16_t)(txq_tail - txq_head);
}

static uint16_t iec104_txq_read_length(uint16_t offset)
{
    return (uint16_t)txq_buf[offset] | (uint16_t)((uint16_t)txq_buf[offset + 1U] << 8U);
}

static void iec104_txq_write_length(uint16_t offset, uint16_t length)
{
    txq_buf[offset] = (uint8_t)length;
    txq_buf[offset + 1U] = (uint8_t)(length >> 8U);
}

void iec104_txq_init(void)
{
    txq_head = 0U;
    txq_tail = 0U;
}

uint8_t iec104_txq_push(const uint8_t *apdu, uint16_t apdu_length)
{
    uint16_t needed;

    if ((apdu == NULL) || (apdu_length == 0U) || (apdu_length > IEC104_MAX_FRAME_LENGTH)) {
        return 0U;
    }

    needed = (uint16_t)(IEC104_TXQ_PREFIX_SIZE + apdu_length);
    if (needed > (uint16_t)(IEC104_TXQ_BUFFER_SIZE - iec104_txq_used())) {
        /* 满缓冲：拒绝新帧，交由上层无损重试。 */
        return 0U;
    }

    if (needed > (uint16_t)(IEC104_TXQ_BUFFER_SIZE - txq_tail)) {
        /* 尾部空间不足，整体前移压缩后再写入。 */
        uint16_t used = iec104_txq_used();

        if (txq_head != 0U) {
            memmove(txq_buf, &txq_buf[txq_head], used);
        }
        txq_head = 0U;
        txq_tail = used;
    }

    iec104_txq_write_length(txq_tail, apdu_length);
    memcpy(&txq_buf[txq_tail + IEC104_TXQ_PREFIX_SIZE], apdu, apdu_length);
    txq_tail = (uint16_t)(txq_tail + needed);
    return 1U;
}

uint8_t iec104_txq_pop(uint8_t *apdu, uint16_t apdu_capacity, uint16_t *apdu_length)
{
    uint16_t frame_length;

    if ((apdu == NULL) || (apdu_length == NULL) || (txq_head == txq_tail)) {
        return 0U;
    }

    frame_length = iec104_txq_read_length(txq_head);
    if ((frame_length == 0U) || (frame_length > apdu_capacity)) {
        return 0U;
    }

    memcpy(apdu, &txq_buf[txq_head + IEC104_TXQ_PREFIX_SIZE], frame_length);
    *apdu_length = frame_length;
    txq_head = (uint16_t)(txq_head + IEC104_TXQ_PREFIX_SIZE + frame_length);
    if (txq_head == txq_tail) {
        txq_head = 0U;
        txq_tail = 0U;
    }
    return 1U;
}
