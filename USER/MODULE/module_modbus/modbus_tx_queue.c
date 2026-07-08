#include "modbus_tx_queue.h"

#include <string.h>

#include "tf_tool.h"

typedef struct {
    uint8_t storage[MODULE_MODBUS_SEND_BUF_SIZE];
    modbus_tx_queue_item_t queue[MODULE_MODBUS_SEND_QUEUE_MAX];
    uint16_t write_index;
    uint8_t head;
    uint8_t tail;
    uint8_t count;
#if MODULE_MODBUS_ENABLE_DIAG
    uint32_t enqueue_count;
    uint32_t dequeue_count;
    uint32_t overwrite_drop_count;
    uint32_t full_drop_count;
    uint32_t invalid_count;
#endif
} modbus_tx_queue_t;

static modbus_tx_queue_t s_tx_queue;

#if MODULE_MODBUS_ENABLE_DIAG
#define MODBUS_TX_QUEUE_DIAG_INC(field_) (s_tx_queue.field_++)
#define MODBUS_TX_QUEUE_DIAG_CLEAR(queue_)       \
    do {                                         \
        (queue_)->enqueue_count = 0U;            \
        (queue_)->dequeue_count = 0U;            \
        (queue_)->overwrite_drop_count = 0U;     \
        (queue_)->full_drop_count = 0U;          \
        (queue_)->invalid_count = 0U;            \
    } while (0)
#else
#define MODBUS_TX_QUEUE_DIAG_INC(field_) ((void)0)
#define MODBUS_TX_QUEUE_DIAG_CLEAR(queue_) ((void)(queue_))
#endif

static uint8_t modbus_tx_queue_next_slot(uint8_t slot)
{
    return (uint8_t)index_increase_loop(slot,
                                        (int32_t)MODULE_MODBUS_SEND_QUEUE_MAX);
}

static uint8_t modbus_tx_queue_previous_slot(uint8_t slot)
{
    return (slot == 0U) ? (uint8_t)(MODULE_MODBUS_SEND_QUEUE_MAX - 1U) :
        (uint8_t)(slot - 1U);
}

void modbus_tx_queue_reset(void)
{
    modbus_tx_queue_t *queue = &s_tx_queue;

    queue->write_index = 0U;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    MODBUS_TX_QUEUE_DIAG_CLEAR(queue);
}

static bool modbus_tx_queue_item_overlap(
    const modbus_tx_queue_item_t *item,
    uint16_t start,
    uint16_t len)
{
    uint32_t item_start;
    uint32_t item_end;
    uint32_t new_start;
    uint32_t new_end;

    if ((item == NULL) || (len == 0U) || (item->len == 0U)) {
        return false;
    }

    item_start = item->buf_index;
    item_end = item_start + item->len;
    new_start = start;
    new_end = new_start + len;

    return ((item_start < new_end) && (new_start < item_end));
}

static void modbus_tx_queue_remove_slot(modbus_tx_queue_t *queue, uint8_t slot)
{
    uint8_t move;
    uint8_t next;

    if ((queue == NULL) || (queue->count == 0U)) {
        return;
    }

    move = slot;
    while (move != queue->tail) {
        next = modbus_tx_queue_next_slot(move);
        if (next == queue->tail) {
            break;
        }
        queue->queue[move] = queue->queue[next];
        move = next;
    }

    queue->tail = modbus_tx_queue_previous_slot(queue->tail);
    queue->count--;
}

static void modbus_tx_queue_pop(void)
{
    modbus_tx_queue_t *queue = &s_tx_queue;

    if (queue->count == 0U) {
        return;
    }

    queue->head = modbus_tx_queue_next_slot(queue->head);
    queue->count--;
    MODBUS_TX_QUEUE_DIAG_INC(dequeue_count);
}

static void modbus_tx_queue_drop_overlap_items(
    modbus_tx_queue_t *queue,
    uint16_t start,
    uint16_t len)
{
    uint8_t offset = 0U;
    uint8_t slot;

    if ((queue == NULL) || (len == 0U)) {
        return;
    }

    slot = queue->head;
    while (offset < queue->count) {
        if (modbus_tx_queue_item_overlap(&queue->queue[slot], start, len)) {
            modbus_tx_queue_remove_slot(queue, slot);
            MODBUS_TX_QUEUE_DIAG_INC(overwrite_drop_count);
        } else {
            slot = modbus_tx_queue_next_slot(slot);
            offset++;
        }
    }
}

bool modbus_tx_queue_enqueue(const uint8_t *frame,
                             uint16_t len,
                             const modbus_response_expect_t *response)
{
    modbus_tx_queue_t *queue = &s_tx_queue;
    uint16_t write_index;

    if ((frame == NULL) || (response == NULL) || (len == 0U) ||
        (len > MODULE_MODBUS_SEND_BUF_SIZE)) {
        MODBUS_TX_QUEUE_DIAG_INC(invalid_count);
        return false;
    }

    write_index = queue->write_index;
    if (((uint32_t)MODULE_MODBUS_SEND_BUF_SIZE - write_index) < len) {
        write_index = 0U;
    }

    modbus_tx_queue_drop_overlap_items(queue, write_index, len);
    while (queue->count >= MODULE_MODBUS_SEND_QUEUE_MAX) {
        modbus_tx_queue_pop();
        MODBUS_TX_QUEUE_DIAG_INC(full_drop_count);
    }

    memcpy(&queue->storage[write_index], frame, len);
    queue->queue[queue->tail].buf_index = write_index;
    queue->queue[queue->tail].len = len;
    queue->queue[queue->tail].response = *response;
    queue->tail = modbus_tx_queue_next_slot(queue->tail);
    queue->count++;
    MODBUS_TX_QUEUE_DIAG_INC(enqueue_count);

    write_index = (uint16_t)(write_index + len);
    queue->write_index = (write_index >= MODULE_MODBUS_SEND_BUF_SIZE) ?
        0U : write_index;
    return true;
}

static bool modbus_tx_queue_peek(modbus_tx_queue_item_t *item)
{
    const modbus_tx_queue_t *queue = &s_tx_queue;

    if ((item == NULL) || (queue->count == 0U)) {
        return false;
    }

    *item = queue->queue[queue->head];
    return true;
}

static bool modbus_tx_queue_copy_frame(const modbus_tx_queue_item_t *item,
                                       uint8_t *out,
                                       uint16_t out_size)
{
    modbus_tx_queue_t *queue = &s_tx_queue;
    uint32_t end_index;

    if ((item == NULL) || (out == NULL) || (item->len == 0U) ||
        (item->len > out_size)) {
        MODBUS_TX_QUEUE_DIAG_INC(invalid_count);
        return false;
    }

    end_index = (uint32_t)item->buf_index + item->len;
    if (end_index > MODULE_MODBUS_SEND_BUF_SIZE) {
        MODBUS_TX_QUEUE_DIAG_INC(invalid_count);
        return false;
    }

    memcpy(out, &queue->storage[item->buf_index], item->len);
    return true;
}

bool modbus_tx_queue_dequeue(uint8_t *frame,
                             uint16_t frame_capacity,
                             uint16_t *frame_len,
                             modbus_response_expect_t *response)
{
    modbus_tx_queue_item_t item;

    if ((frame == NULL) || (frame_len == NULL) || (response == NULL)) {
        MODBUS_TX_QUEUE_DIAG_INC(invalid_count);
        return false;
    }

    if (!modbus_tx_queue_peek(&item)) {
        return false;
    }

    if (!modbus_tx_queue_copy_frame(&item, frame, frame_capacity)) {
        modbus_tx_queue_pop();
        return false;
    }

    *response = item.response;
    *frame_len = item.len;
    modbus_tx_queue_pop();
    return true;
}

uint8_t modbus_tx_queue_count(void)
{
    return s_tx_queue.count;
}

#if MODULE_MODBUS_ENABLE_DIAG
void modbus_tx_queue_get_diag(modbus_tx_queue_diag_t *diag)
{
    modbus_tx_queue_t *queue = &s_tx_queue;

    if (diag == NULL) {
        return;
    }

    diag->write_index = queue->write_index;
    diag->enqueue_count = queue->enqueue_count;
    diag->dequeue_count = queue->dequeue_count;
    diag->overwrite_drop_count = queue->overwrite_drop_count;
    diag->full_drop_count = queue->full_drop_count;
    diag->invalid_count = queue->invalid_count;
}

void modbus_tx_queue_clear_diag(void)
{
    modbus_tx_queue_t *queue = &s_tx_queue;

    MODBUS_TX_QUEUE_DIAG_CLEAR(queue);
}
#endif
