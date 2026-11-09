#include "COMMON/ringbuf.h"

typedef enum {
    RINGBUF_EMPTY,
    RINGBUF_FULL,
    RINGBUF_HALFFULL,
} ringbuf_state_t;

static ringbuf_state_t ringbuf_state(const ringbuf_t *rb)
{
    if (rb == 0) {
        return RINGBUF_EMPTY;
    }

    if (rb->read_index == rb->write_index) {
        return (rb->read_mirror == rb->write_mirror) ? RINGBUF_EMPTY : RINGBUF_FULL;
    }

    return RINGBUF_HALFFULL;
}

static uint32_t ringbuf_forward_index(uint32_t index, uint32_t step, uint32_t size, uint32_t *mirror)
{
    index += step;
    if (index >= size) {
        index -= size;
        if (mirror != 0) {
            *mirror = ~(*mirror) & 0x01U;
        }
    }

    return index;
}

void ringbuf_init(ringbuf_t *rb, uint8_t *buf, uint32_t size)
{
    if (rb == 0) {
        return;
    }

    rb->buf = buf;
    rb->read_mirror = 0U;
    rb->read_index = 0U;
    rb->write_mirror = 0U;
    rb->write_index = 0U;
    rb->buffer_size = size;
    rb->overflow = 0U;
}

void ringbuf_reset(ringbuf_t *rb)
{
    if (rb == 0) {
        return;
    }

    rb->read_mirror = 0U;
    rb->read_index = 0U;
    rb->write_mirror = 0U;
    rb->write_index = 0U;
    rb->overflow = 0U;
}

uint32_t ringbuf_capacity(const ringbuf_t *rb)
{
    return (rb != 0) ? rb->buffer_size : 0U;
}

uint32_t ringbuf_available(const ringbuf_t *rb)
{
    if (rb == 0) {
        return 0U;
    }

    switch (ringbuf_state(rb)) {
    case RINGBUF_EMPTY:
        return 0U;
    case RINGBUF_FULL:
        return rb->buffer_size;
    case RINGBUF_HALFFULL:
    default:
        if (rb->write_index > rb->read_index) {
            return rb->write_index - rb->read_index;
        }
        return rb->buffer_size - (rb->read_index - rb->write_index);
    }
}

uint32_t ringbuf_free(const ringbuf_t *rb)
{
    uint32_t available;

    if (rb == 0) {
        return 0U;
    }

    available = ringbuf_available(rb);
    return (available <= rb->buffer_size) ? (rb->buffer_size - available) : 0U;
}

uint32_t ringbuf_overflow_count(const ringbuf_t *rb)
{
    return (rb != 0) ? rb->overflow : 0U;
}

uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t space;
    uint32_t written = 0U;

    if ((rb == 0) || (rb->buf == 0) || (rb->buffer_size == 0U) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    space = ringbuf_free(rb);
    if (space == 0U) {
        rb->overflow += len;
        return 0U;
    }
    if (len > space) {
        rb->overflow += (len - space);
        len = space;
    }

    while (written < len) {
        rb->buf[rb->write_index] = data[written];
        rb->write_index = ringbuf_forward_index(rb->write_index, 1U, rb->buffer_size, &rb->write_mirror);
        written++;
    }

    return written;
}

uint32_t ringbuf_write_force(ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t original_len = len;
    uint32_t written = 0U;

    if ((rb == 0) || (rb->buf == 0) || (rb->buffer_size == 0U) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    if (len > rb->buffer_size) {
        data = &data[len - rb->buffer_size];
        len = rb->buffer_size;
        rb->overflow += (original_len - len);
    }

    while (written < len) {
        if (ringbuf_free(rb) == 0U) {
            (void)ringbuf_drop(rb, 1U);
            rb->overflow++;
        }

        rb->buf[rb->write_index] = data[written];
        rb->write_index = ringbuf_forward_index(rb->write_index, 1U, rb->buffer_size, &rb->write_mirror);
        written++;
    }

    return written;
}

uint32_t ringbuf_read_exact(ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    if ((rb == 0) || (data == 0) || (len == 0U) || (ringbuf_available(rb) < len)) {
        return 0U;
    }

    return ringbuf_read(rb, data, len);
}

uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    uint32_t available;
    uint32_t read_len = 0U;

    if ((rb == 0) || (rb->buf == 0) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    available = ringbuf_available(rb);
    if (len > available) {
        len = available;
    }

    while (read_len < len) {
        data[read_len] = rb->buf[rb->read_index];
        rb->read_index = ringbuf_forward_index(rb->read_index, 1U, rb->buffer_size, &rb->read_mirror);
        read_len++;
    }

    return read_len;
}

uint32_t ringbuf_peek(const ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    uint32_t available;
    uint32_t read_len = 0U;
    uint32_t read_index;
    uint32_t read_mirror;

    if ((rb == 0) || (rb->buf == 0) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    available = ringbuf_available(rb);
    if (len > available) {
        len = available;
    }

    read_index = rb->read_index;
    read_mirror = rb->read_mirror;

    while (read_len < len) {
        data[read_len] = rb->buf[read_index];
        read_index = ringbuf_forward_index(read_index, 1U, rb->buffer_size, &read_mirror);
        read_len++;
    }

    return read_len;
}

uint32_t ringbuf_drop(ringbuf_t *rb, uint32_t len)
{
    uint32_t available;
    uint32_t drop_len = 0U;

    if ((rb == 0) || (rb->buf == 0) || (len == 0U)) {
        return 0U;
    }

    available = ringbuf_available(rb);
    if (len > available) {
        len = available;
    }

    while (drop_len < len) {
        rb->read_index = ringbuf_forward_index(rb->read_index, 1U, rb->buffer_size, &rb->read_mirror);
        drop_len++;
    }

    return drop_len;
}
