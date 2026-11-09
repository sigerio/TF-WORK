#include "COMM/modbus/modbus_txq.h"

#include <string.h>

#include "COMMON/circular_index.h"
#include "COMM/modbus/devices/modbus_wanrong.h"

typedef struct {
    uint8_t frame_buf[MODULE_MODBUS_SEND_BUF_SIZE];
    modbus_txq_item_t items[MODULE_MODBUS_SEND_QUEUE_MAX];
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
} modbus_txq_t;

static modbus_txq_t transmit_queue;

#if MODULE_MODBUS_ENABLE_DIAG
#define MODBUS_TXQ_DIAG_INC(field_) (transmit_queue.field_++)
#define MODBUS_TXQ_DIAG_CLEAR(queue_)       \
    do {                                       \
        (queue_)->enqueue_count = 0U;          \
        (queue_)->dequeue_count = 0U;          \
        (queue_)->overwrite_drop_count = 0U;   \
        (queue_)->full_drop_count = 0U;        \
        (queue_)->invalid_count = 0U;          \
    } while (0)
#else
#define MODBUS_TXQ_DIAG_INC(field_) ((void)0)
#define MODBUS_TXQ_DIAG_CLEAR(queue_) ((void)(queue_))
#endif

static uint8_t modbus_txq_next_slot(uint8_t slot)
{
    return (uint8_t)index_increase_loop(slot, (int32_t)MODULE_MODBUS_SEND_QUEUE_MAX);
}

static uint8_t modbus_txq_prev_slot(uint8_t slot)
{
    return (slot == 0U) ? (uint8_t)(MODULE_MODBUS_SEND_QUEUE_MAX - 1U) :
        (uint8_t)(slot - 1U);
}

void modbus_txq_reset(void)
{
    modbus_txq_t *txq = &transmit_queue;

    txq->write_pos = 0U;
    txq->head_slot = 0U;
    txq->tail_slot = 0U;
    txq->item_count = 0U;
    MODBUS_TXQ_DIAG_CLEAR(txq);
}

static bool modbus_txq_item_overlaps(const modbus_txq_item_t *queued_frame, uint16_t start, uint16_t frame_len)
{
    uint32_t item_start;
    uint32_t item_end;
    uint32_t new_start;
    uint32_t new_end;

    if ((queued_frame == NULL) || (frame_len == 0U) || (queued_frame->frame_len == 0U)) {
        return false;
    }

    item_start = queued_frame->frame_pos;
    item_end = item_start + queued_frame->frame_len;
    new_start = start;
    new_end = new_start + frame_len;

    return ((item_start < new_end) && (new_start < item_end));
}

static void modbus_txq_remove_slot(modbus_txq_t *txq, uint8_t slot)
{
    uint8_t move;
    uint8_t next;

    if ((txq == NULL) || (txq->item_count == 0U)) {
        return;
    }

    move = slot;
    while (move != txq->tail_slot) {
        next = modbus_txq_next_slot(move);
        if (next == txq->tail_slot) {
            break;
        }
        txq->items[move] = txq->items[next];
        move = next;
    }

    txq->tail_slot = modbus_txq_prev_slot(txq->tail_slot);
    txq->item_count--;
}

void modbus_txq_drop_wanrong_motion(modbus_dev_type_t device)
{
    modbus_txq_t *txq = &transmit_queue;
    uint8_t slot = txq->head_slot;
    uint8_t offset = 0U;

    while (offset < txq->item_count) {
        const modbus_rsp_expect_t *expect = &txq->items[slot].expect;
        if (expect->dev_type == device && expect->req_type == MODBUS_REQ_WRITE_REG &&
            expect->echo_addr == WANRONG_REG_COMMAND && expect->echo_value != WANRONG_STOP) {
            modbus_txq_remove_slot(txq, slot);
        } else {
            slot = modbus_txq_next_slot(slot);
            offset++;
        }
    }
}

static void modbus_txq_drop_head(void)
{
    modbus_txq_t *txq = &transmit_queue;

    if (txq->item_count == 0U) {
        return;
    }

    txq->head_slot = modbus_txq_next_slot(txq->head_slot);
    txq->item_count--;
    MODBUS_TXQ_DIAG_INC(dequeue_count);
}

static void modbus_txq_drop_overlaps(modbus_txq_t *txq, uint16_t start, uint16_t frame_len)
{
    uint8_t offset = 0U;
    uint8_t slot;

    if ((txq == NULL) || (frame_len == 0U)) {
        return;
    }

    slot = txq->head_slot;
    while (offset < txq->item_count) {
        if (modbus_txq_item_overlaps(&txq->items[slot], start, frame_len)) {
            modbus_txq_remove_slot(txq, slot);
            MODBUS_TXQ_DIAG_INC(overwrite_drop_count);
        } else {
            slot = modbus_txq_next_slot(slot);
            offset++;
        }
    }
}

bool modbus_txq_push(const uint8_t *frame, uint16_t frame_len, const modbus_rsp_expect_t *expect)
{
    modbus_txq_t *txq = &transmit_queue;
    uint16_t write_pos;

    if ((frame == NULL) || (expect == NULL) || (frame_len == 0U) ||
        (frame_len > MODULE_MODBUS_SEND_BUF_SIZE) || ((uint32_t)expect->bus >= (uint32_t)MODBUS_BUS_COUNT)) {
        MODBUS_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    write_pos = txq->write_pos;
    if (((uint32_t)MODULE_MODBUS_SEND_BUF_SIZE - write_pos) < frame_len) {
        write_pos = 0U;
    }

    modbus_txq_drop_overlaps(txq, write_pos, frame_len);
    while (txq->item_count >= MODULE_MODBUS_SEND_QUEUE_MAX) {
        modbus_txq_drop_head();
        MODBUS_TXQ_DIAG_INC(full_drop_count);
    }

    memcpy(&txq->frame_buf[write_pos], frame, frame_len);
    txq->items[txq->tail_slot].frame_pos = write_pos;
    txq->items[txq->tail_slot].frame_len = frame_len;
    txq->items[txq->tail_slot].expect = *expect;
    txq->tail_slot = modbus_txq_next_slot(txq->tail_slot);
    txq->item_count++;
    MODBUS_TXQ_DIAG_INC(enqueue_count);

    write_pos = (uint16_t)(write_pos + frame_len);
    txq->write_pos = (write_pos >= MODULE_MODBUS_SEND_BUF_SIZE) ? 0U : write_pos;
    return true;
}

static bool modbus_txq_peek(modbus_bus_t bus, modbus_txq_item_t *out_item)
{
    const modbus_txq_t *txq = &transmit_queue;
    uint8_t slot;
    uint8_t offset = 0U;

    if ((out_item == NULL) || (txq->item_count == 0U)) {
        return false;
    }

    slot = txq->head_slot;
    while (offset < txq->item_count) {
        if (txq->items[slot].expect.bus == bus) {
            *out_item = txq->items[slot];
            return true;
        }
        slot = modbus_txq_next_slot(slot);
        offset++;
    }

    return false;
}

static bool modbus_txq_copy_frame(const modbus_txq_item_t *queued_frame, uint8_t *out, uint16_t out_size)
{
    modbus_txq_t *txq = &transmit_queue;
    uint32_t end_index;

    if ((queued_frame == NULL) || (out == NULL) || (queued_frame->frame_len == 0U) || (queued_frame->frame_len > out_size)) {
        MODBUS_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    end_index = (uint32_t)queued_frame->frame_pos + queued_frame->frame_len;
    if (end_index > MODULE_MODBUS_SEND_BUF_SIZE) {
        MODBUS_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    memcpy(out, &txq->frame_buf[queued_frame->frame_pos], queued_frame->frame_len);
    return true;
}

static void modbus_txq_drop_first_of_bus(modbus_bus_t bus)
{
    modbus_txq_t *txq = &transmit_queue;
    uint8_t slot;
    uint8_t offset = 0U;

    slot = txq->head_slot;
    while (offset < txq->item_count) {
        if (txq->items[slot].expect.bus == bus) {
            modbus_txq_remove_slot(txq, slot);
            MODBUS_TXQ_DIAG_INC(dequeue_count);
            return;
        }
        slot = modbus_txq_next_slot(slot);
        offset++;
    }
}

bool modbus_txq_pop(modbus_bus_t bus, uint8_t *frame, uint16_t frame_capacity, uint16_t *frame_len, modbus_rsp_expect_t *expect)
{
    modbus_txq_item_t queued_frame;

    if ((frame == NULL) || (frame_len == NULL) || (expect == NULL)) {
        MODBUS_TXQ_DIAG_INC(invalid_count);
        return false;
    }

    if (!modbus_txq_peek(bus, &queued_frame)) {
        return false;
    }

    if (!modbus_txq_copy_frame(&queued_frame, frame, frame_capacity)) {
        modbus_txq_drop_first_of_bus(bus);
        return false;
    }

    *expect = queued_frame.expect;
    *frame_len = queued_frame.frame_len;
    modbus_txq_drop_first_of_bus(bus);
    return true;
}

uint8_t modbus_txq_count(void)
{
    return transmit_queue.item_count;
}

#if MODULE_MODBUS_ENABLE_DIAG
void modbus_txq_get_diag(modbus_txq_diag_t *diag)
{
    modbus_txq_t *txq = &transmit_queue;

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

void modbus_txq_clear_diag(void)
{
    modbus_txq_t *txq = &transmit_queue;

    MODBUS_TXQ_DIAG_CLEAR(txq);
}
#endif
