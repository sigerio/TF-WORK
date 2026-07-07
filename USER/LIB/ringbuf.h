#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    uint32_t read_mirror;
    uint32_t read_index;
    uint32_t write_mirror;
    uint32_t write_index;
    uint32_t buffer_size;
    uint32_t overflow;
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb, uint8_t *buf, uint32_t size);
void ringbuf_reset(ringbuf_t *rb);
uint32_t ringbuf_capacity(const ringbuf_t *rb);
uint32_t ringbuf_available(const ringbuf_t *rb);
uint32_t ringbuf_free(const ringbuf_t *rb);
uint32_t ringbuf_overflow_count(const ringbuf_t *rb);
uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t ringbuf_write_force(ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len);
uint32_t ringbuf_peek(const ringbuf_t *rb, uint8_t *data, uint32_t len);
uint32_t ringbuf_drop(ringbuf_t *rb, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* RINGBUF_H */
