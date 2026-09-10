#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "CONTROL/ws2812/driver/ws2812_driver.h"

int main(void)
{
    /* RGB input followed by expected BRG wire bytes; mixed values also check bit order. */
    const uint8_t cases[][6] = {
        {255, 0, 0, 0, 255, 0},
        {0, 255, 0, 0, 0, 255},
        {0, 0, 255, 255, 0, 0},
        {0x12, 0x34, 0x56, 0x56, 0x12, 0x34},
    };
    uint8_t frame[2U * WS2812_ENCODED_BYTES_PER_LED];

    for (unsigned index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        memset(frame, 0xA5, sizeof(frame));
        ws2812_driver_encode_pixel(frame, 1U, cases[index][0], cases[index][1], cases[index][2]);
        for (unsigned bit = 0; bit < WS2812_ENCODED_BYTES_PER_LED; bit++) {
            uint8_t expected = (cases[index][3U + bit / 8U] & (0x80U >> (bit % 8U))) ? 0xF0U : 0xC0U;

            assert(frame[bit] == 0xA5U);
            assert(frame[WS2812_ENCODED_BYTES_PER_LED + bit] == expected);
        }
    }
    return 0;
}
