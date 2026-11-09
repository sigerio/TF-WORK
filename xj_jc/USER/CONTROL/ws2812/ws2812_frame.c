#include "CONTROL/ws2812/ws2812_frame.h"

#include <stddef.h>
#include <string.h>

void ws2812_frame_clear(ws2812_frame_t *frame)
{
    if (frame != NULL && frame->pixels != NULL) {
        memset(frame->pixels, 0, frame->led_count * sizeof(frame->pixels[0]));
    }
}

void ws2812_frame_fill(ws2812_frame_t *frame, uint32_t first, uint32_t last, uint32_t color)
{
    if (frame == NULL || frame->pixels == NULL || first >= frame->led_count || last >= frame->led_count) {
        return;
    }
    uint32_t index = first;
    for (;;) {
        frame->pixels[index] = color & 0xFFFFFFU;
        if (index == last) {
            break;
        }
        index = (index + 1U) % frame->led_count;
    }
}
