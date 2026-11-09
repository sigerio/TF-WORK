#ifndef WS2812_BASIC_H
#define WS2812_BASIC_H

#include "CONTROL/ws2812/ws2812_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 动画状态由调用者持有，初值为 0；跑马 length 由调用者指定。 */
void ws2812_effect_solid(ws2812_frame_t *frame, uint32_t first, uint32_t last, uint32_t color, uint16_t brightness);
void ws2812_effect_off(ws2812_frame_t *frame, uint32_t first, uint32_t last);
void ws2812_effect_breath(ws2812_frame_t *frame, uint32_t first, uint32_t last, uint32_t color, ws2812_breath_state_t *state);
void ws2812_effect_flash(ws2812_frame_t *frame, uint32_t first, uint32_t last, uint32_t color1,
    uint32_t color2, int8_t cycle_limit, uint8_t brightness, const ws2812_flash_state_t *state);
void ws2812_flash_update(uint8_t color1_ticks, uint8_t color2_ticks, int8_t cycle_limit, ws2812_flash_state_t *state);
void ws2812_effect_trail(ws2812_frame_t *frame, uint32_t color, uint8_t step_ticks, ws2812_trail_state_t *state);
void ws2812_effect_wave(ws2812_frame_t *frame, uint32_t color, uint8_t width, uint8_t step_ticks, ws2812_wave_state_t *state);

/* 整条双闪：亮 150 ms / 灭 150 ms / 亮 150 ms / 灭 1050 ms，周期 1500 ms。 */
void ws2812_effect_double_flash(ws2812_frame_t *frame, uint32_t color, uint32_t elapsed_ms);
/* 两半交替亮，每 500 ms 换边；奇数灯珠前半多一颗，单颗时保持常亮。 */
void ws2812_effect_alternating_halves(ws2812_frame_t *frame, uint32_t color, uint32_t elapsed_ms);

typedef enum {
    WS2812_BASIC_WHITE_TRAILING = 1,
    WS2812_BASIC_GREEN_FLASH_THEN_STEADY,
    WS2812_BASIC_YELLOW_STEADY,
    WS2812_BASIC_RED_FLASH,
    WS2812_BASIC_RED_STEADY,
    WS2812_BASIC_WHITE_BREATHING,
} ws2812_basic_effect_e;

/* 原六种基础效果，elapsed_ms 为播放时间，不依赖调试开关。 */
void ws2812_render_basic_effect(ws2812_frame_t *frame, ws2812_basic_effect_e effect, uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif
#endif /* WS2812_BASIC_H */
