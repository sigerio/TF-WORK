#include <assert.h>
#include <stdint.h>

#include "ringbuf.h"

int main(void)
{
    uint8_t storage[8];
    uint8_t source[] = {0x68U, 0x04U, 0x07U};
    uint8_t data[4];
    ringbuf_t fifo;

    ringbuf_init(&fifo, storage, sizeof(storage));
    assert(ringbuf_write(&fifo, source, sizeof(source)) == sizeof(source));
    assert(ringbuf_read_exact(&fifo, data, sizeof(data)) == 0U);
    assert(ringbuf_available(&fifo) == sizeof(source));
    assert(ringbuf_peek(&fifo, data, sizeof(source)) == sizeof(source));
    assert(ringbuf_read_exact(&fifo, data, sizeof(source)) == sizeof(source));
    assert(ringbuf_available(&fifo) == 0U);
    return 0;
}
