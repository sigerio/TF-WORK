#include "modbus_tx_queue.h"

#include <string.h>

static uint8_t modbus_tx_queue_next_index(uint8_t index)
{
    index++;
    if (index >= MODULE_MODBUS_SEND_QUEUE_MAX) {
        index = 0U;
    }

    return index;
}

static uint8_t modbus_tx_queue_prev_index(uint8_t index)
{
    return (index == 0U) ? (uint8_t)(MODULE_MODBUS_SEND_QUEUE_MAX - 1U) :
        (uint8_t)(index - 1U);
}

static uint8_t modbus_tx_queue_index_at(const modbus_tx_queue_t *queue,
                                        uint8_t offset)
{
    uint8_t index = queue->head;

    while (offset > 0U) {
        index = modbus_tx_queue_next_index(index);
        offset--;
    }

    return index;
}

void modbus_tx_queue_reset(modbus_tx_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    queue->write_index = 0U;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    queue->enqueue_count = 0U;
    queue->dequeue_count = 0U;
    queue->overwrite_drop_count = 0U;
    queue->full_drop_count = 0U;
    queue->invalid_count = 0U;
}

static bool modbus_tx_queue_frame_overlap(const modbus_tx_queue_item_t *item,
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

static void modbus_tx_queue_remove_at(modbus_tx_queue_t *queue, uint8_t index)
{
    uint8_t move;
    uint8_t next;

    if ((queue == NULL) || (queue->count == 0U)) {
        return;
    }

    move = index;
    while (move != queue->tail) {
        next = modbus_tx_queue_next_index(move);
        if (next == queue->tail) {
            break;
        }
        queue->queue[move] = queue->queue[next];
        move = next;
    }

    queue->tail = modbus_tx_queue_prev_index(queue->tail);
    queue->count--;
}

void modbus_tx_queue_pop(modbus_tx_queue_t *queue)
{
    if ((queue == NULL) || (queue->count == 0U)) {
        return;
    }

    queue->head = modbus_tx_queue_next_index(queue->head);
    queue->count--;
    queue->dequeue_count++;
}

static void modbus_tx_queue_drop_overlapped(modbus_tx_queue_t *queue,
                                            uint16_t start,
                                            uint16_t len)
{
    uint8_t offset = 0U;

    if ((queue == NULL) || (len == 0U)) {
        return;
    }

    while (offset < queue->count) {
        uint8_t index = modbus_tx_queue_index_at(queue, offset);

        if (modbus_tx_queue_frame_overlap(&queue->queue[index], start, len)) {
            modbus_tx_queue_remove_at(queue, index);
            queue->overwrite_drop_count++;
        } else {
            offset++;
        }
    }
}

bool modbus_tx_queue_push(modbus_tx_queue_t *queue,
                          const uint8_t *frame,
                          uint16_t len,
                          const modbus_response_expect_t *response)
{
    uint16_t write_index;

    if ((queue == NULL) || (frame == NULL) || (response == NULL) ||
        (len == 0U) || (len > MODULE_MODBUS_SEND_BUF_SIZE)) {
        if (queue != NULL) {
            queue->invalid_count++;
        }
        return false;
    }

    write_index = queue->write_index;
    if (((uint32_t)MODULE_MODBUS_SEND_BUF_SIZE - write_index) < len) {
        write_index = 0U;
    }

    modbus_tx_queue_drop_overlapped(queue, write_index, len);
    while (queue->count >= MODULE_MODBUS_SEND_QUEUE_MAX) {
        modbus_tx_queue_pop(queue);
        queue->full_drop_count++;
    }

    memcpy(&queue->storage[write_index], frame, len);
    queue->queue[queue->tail].buf_index = write_index;
    queue->queue[queue->tail].len = len;
    queue->queue[queue->tail].response = *response;
    queue->tail = modbus_tx_queue_next_index(queue->tail);
    queue->count++;
    queue->enqueue_count++;

    write_index = (uint16_t)(write_index + len);
    queue->write_index = (write_index >= MODULE_MODBUS_SEND_BUF_SIZE) ? 0U : write_index;
    return true;
}

bool modbus_tx_queue_peek(const modbus_tx_queue_t *queue,
                          modbus_tx_queue_item_t *item)
{
    if ((queue == NULL) || (item == NULL) || (queue->count == 0U)) {
        return false;
    }

    *item = queue->queue[queue->head];
    return true;
}
