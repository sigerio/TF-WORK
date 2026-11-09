#ifndef WS2812_TYPES_H
#define WS2812_TYPES_H

#include <stdint.h>

/* 逻辑颜色统一为 0xRRGGBB；灯带 BRG 线序只由驱动处理。 */
#define WS2812_RGB(r, g, b) ((((uint32_t)(r) & 255U) << 16U) | (((uint32_t)(g) & 255U) << 8U) | ((uint32_t)(b) & 255U))

/* 缓冲区由调用者持有，至少包含 led_count 个颜色值。 */
typedef struct {
    uint32_t *pixels;
    uint32_t led_count;
} ws2812_frame_t;

typedef struct {
    uint8_t stage;
    uint8_t elapsed_ticks;
    uint8_t completed_cycles;
} ws2812_flash_state_t;

typedef struct {
    uint32_t head;
    uint32_t length;
    uint8_t elapsed_ticks;
} ws2812_trail_state_t;

typedef struct {
    uint8_t head;
    uint8_t elapsed_ticks;
} ws2812_wave_state_t;

typedef struct {
    uint8_t table_index;
} ws2812_breath_state_t;

#endif /* WS2812_TYPES_H */
