#include "tf_485_txq.h"

#include <string.h>

#include "tf_tool.h"

typedef struct {
    uint8_t frame_buf[MODULE_MODBUS_SEND_BUF_SIZE];
    tf_485_txq_item_t items[MODULE_MODBUS_SEND_QUEUE_MAX];
    uint16_t write_pos;
    uint8_t head_slot;
    uint8_t tail_slot;
    uint8_t item_count;
#if MODULE_MODBUS_ENABLE_DIAG
    uint32_t enqueue_count;
    uint32_t dequeue_count;
    uint32_t overwrite_drop_count;
    uint32_t full_drop_count;
    uint32_t invalid_count;
#endif
} tf_485_txq_t;

static tf_485_txq_t s_txq;

#if MODULE_MODBUS_ENABLE_DIAG
#define TF_485_TXQ_DIAG_INC(field_) (s_txq.field_++)
#define TF_485_TXQ_DIAG_CLEAR(queue_)       \
    do {                                         \
        (queue_)->enqueue_count = 0U;            \
        (queue_)->dequeue_count = 0U;            \
        (queue_)->overwrite_drop_count = 0U;     \
        (queue_)->full_drop_count = 0U;          \
        (queue_)->invalid_count = 0U;            \
    } while (0)
#else
#define TF_485_TXQ_DIAG_INC(field_) ((void)0)
#define TF_485_TXQ_DIAG_CLEAR(queue_) ((void)(queue_))
#endif

static uint8_t tf_485_txq_next_slot(uint8_t slot)
{
    return (uint8_t)index_increase_loop(slot,
                                        (int32_t)MODULE_MODBUS_SEND_QUEUE_MAX);
}

static uint8_t tf_485_txq_prev_slot(uint8_t slot)
{
    return (slot == 0U) ? (uint8_t)(MODULE_MODBUS_SEND_QUEUE_MAX - 1U) :
        (uint8_t)(slot - 1U);
}

void tf_485_txq_reset(void)
{
    tf_485_txq_t *txq = &s_txq;

    txq->write_pos = 0U;
    txq->head_slot = 0U;
    txq->tail_slot = 0U;
    txq->item_count = 0U;
    TF_485_TXQ_DIAG_CLEAR(txq);
}

static bool tf_485_txq_item_overlaps(
    const tf_485_txq_item_t *queued_frame,
    uint16_t start,
    uint16_t frame_len)
{
    uint32_t item_start;
    uint32_t item_end;
    uint32_t new_start;
    uint32_t new_end;

    if ((queued_frame == NULL) || (frame_len == 0U) ||
        (queued_frame->frame_len == 0U)) {
        return false;
    }

    item_start = queued_frame->frame_pos;
    item_end = item_start + queued_frame->frame_len;
    new_start = start;
    new_end = new_start + frame_len;

    return ((item_start < new_end) && (new_start < item_end));
}

static void tf_485_txq_remove_slot(tf_485_txq_t *txq, uint8_t slot)
{
    uint8_t move;
    uint8_t next;

    if ((txq == NULL) || (txq->item_count == 0U)) {
        return;
    }

    move = slot;
    while (move != txq->tail_slot) {
        next = tf_485_txq_next_slot(move);
        if (next == txq->tail_slot) {
            break;
        }
        txq->items[move] = txq->items[next];
        move = next;
    }

    txq->tail_slot = tf_485_txq_prev_slot(txq->tail_slot);
    txq->item_count--;
}

static void tf_485_txq_drop_head(void)
{
    tf_485_txq_t *txq = &s_txq;

    if (txq->item_count == 0U) {
        return;
    }

    txq->head_slot = tf_485_txq_next_slot(txq->head_slot);
    txq->item_count--;
    TF_485_TXQ_DIAG_INC(dequeue_count);
}

static void tf_485_txq_drop_overlaps(
    tf_485_txq_t *txq,
    uint16_t start,
    uint16_t frame_len)
{
    uint8_t offset = 0U;
    uint8_t slot;

    if ((txq == NULL) || (frame_len == 0U)) {
        return;
    }

    slot = txq->head_slot;
    while (offset < txq->item_count) {
        if (tf_485_txq_item_overlaps(&txq->items[slot], start, frame_len)) {
            tf_485_txq_remove_slot(txq, slot);
            TF_485_TXQ_DIAG_INC(overwrite_drop_count);
        } else {
            slot = tf_485_txq_next_slot(slot);
            offset++;
        }
    }
}

bool tf_485_txq_push(const uint8_t *frame,
                             uint16_t frame_len,
                             const tf_485_rsp_expect_t *expect)
{
    tf_485_txq_t *txq = &s_txq;
    uint16_t write_pos;

    if ((frame == NULL) || (expect == NULL) || (frame_len == 0U) ||
        (frame_len > MODULE_MODBUS_SEND_BUF_SIZE)) {
        TF_485_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    write_pos = txq->write_pos;
    if (((uint32_t)MODULE_MODBUS_SEND_BUF_SIZE - write_pos) < frame_len) {
        write_pos = 0U;
    }

    tf_485_txq_drop_overlaps(txq, write_pos, frame_len);
    while (txq->item_count >= MODULE_MODBUS_SEND_QUEUE_MAX) {
        tf_485_txq_drop_head();
        TF_485_TXQ_DIAG_INC(full_drop_count);
    }

    memcpy(&txq->frame_buf[write_pos], frame, frame_len);
    txq->items[txq->tail_slot].frame_pos = write_pos;
    txq->items[txq->tail_slot].frame_len = frame_len;
    txq->items[txq->tail_slot].expect = *expect;
    txq->tail_slot = tf_485_txq_next_slot(txq->tail_slot);
    txq->item_count++;
    TF_485_TXQ_DIAG_INC(enqueue_count);

    write_pos = (uint16_t)(write_pos + frame_len);
    txq->write_pos = (write_pos >= MODULE_MODBUS_SEND_BUF_SIZE) ?
        0U : write_pos;
    return true;
}

static bool tf_485_txq_peek(tf_485_txq_item_t *out_item)
{
    const tf_485_txq_t *txq = &s_txq;

    if ((out_item == NULL) || (txq->item_count == 0U)) {
        return false;
    }

    *out_item = txq->items[txq->head_slot];
    return true;
}

static bool tf_485_txq_copy_frame(const tf_485_txq_item_t *queued_frame,
                                       uint8_t *out,
                                       uint16_t out_size)
{
    tf_485_txq_t *txq = &s_txq;
    uint32_t end_index;

    if ((queued_frame == NULL) || (out == NULL) ||
        (queued_frame->frame_len == 0U) ||
        (queued_frame->frame_len > out_size)) {
        TF_485_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    end_index = (uint32_t)queued_frame->frame_pos + queued_frame->frame_len;
    if (end_index > MODULE_MODBUS_SEND_BUF_SIZE) {
        TF_485_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    memcpy(out, &txq->frame_buf[queued_frame->frame_pos],
           queued_frame->frame_len);
    return true;
}

bool tf_485_txq_pop(uint8_t *frame,
                             uint16_t frame_capacity,
                             uint16_t *frame_len,
                             tf_485_rsp_expect_t *expect)
{
    tf_485_txq_item_t queued_frame;

    if ((frame == NULL) || (frame_len == NULL) || (expect == NULL)) {
        TF_485_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    if (!tf_485_txq_peek(&queued_frame)) {
        return false;
    }

    if (!tf_485_txq_copy_frame(&queued_frame, frame, frame_capacity)) {
        tf_485_txq_drop_head();
        return false;
    }

    *expect = queued_frame.expect;
    *frame_len = queued_frame.frame_len;
    tf_485_txq_drop_head();
    return true;
}

uint8_t tf_485_txq_count(void)
{
    return s_txq.item_count;
}

#if MODULE_MODBUS_ENABLE_DIAG
void tf_485_txq_get_diag(tf_485_txq_diag_t *diag)
{
    tf_485_txq_t *txq = &s_txq;

    if (diag == NULL) {
        return;
    }

    diag->write_pos = txq->write_pos;
    diag->enqueue_count = txq->enqueue_count;
    diag->dequeue_count = txq->dequeue_count;
    diag->overwrite_drop_count = txq->overwrite_drop_count;
    diag->full_drop_count = txq->full_drop_count;
    diag->invalid_count = txq->invalid_count;
}

void tf_485_txq_clear_diag(void)
{
    tf_485_txq_t *txq = &s_txq;

    TF_485_TXQ_DIAG_CLEAR(txq);
}
#endif
