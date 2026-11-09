#include "CONTROL/ws2812/effects/ws2812_basic.h"
#include "CONTROL/ws2812/ws2812_frame.h"

#include <stddef.h>
#include <stdbool.h>

#define BREATH_MAP_SIZE 100U
#define WS2812_TRAIL_BRIGHTNESS 80U
#define WS2812_WAVE_EDGE_BRIGHTNESS 15U
#define WS2812_WAVE_BRIGHTNESS_STEP_SHIFT 4U

void ws2812_effect_double_flash(ws2812_frame_t *frame, uint32_t color, uint32_t elapsed_ms)
{
    enum { pulse_ms = 150U, gap_ms = 150U, pause_ms = 1050U, cycle_ms = 2U * pulse_ms + gap_ms + pause_ms };
    uint32_t phase = elapsed_ms % cycle_ms;
    bool on = phase < pulse_ms || (phase >= pulse_ms + gap_ms && phase < 2U * pulse_ms + gap_ms);

    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) return;
    ws2812_frame_fill(frame, 0U, frame->led_count - 1U, on ? color : 0U);
}

void ws2812_effect_alternating_halves(ws2812_frame_t *frame, uint32_t color, uint32_t elapsed_ms)
{
    enum { half_cycle_ms = 500U };
    uint32_t split;
    bool first_half = (elapsed_ms / half_cycle_ms) % 2U == 0U;

    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) return;
    split = frame->led_count / 2U + frame->led_count % 2U;
    for (uint32_t i = 0U; i < frame->led_count; i++) {
        frame->pixels[i] = (frame->led_count == 1U || ((i < split) == first_half)) ? color & 0xFFFFFFU : 0U;
    }
}

static const uint16_t breath_map[BREATH_MAP_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03,
    0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x0E, 0x0F, 0x11, 0x13, 0x15, 0x18, 0x1B,
    0x1E, 0x21, 0x25, 0x2A, 0x2F, 0x35, 0x3B, 0x42, 0x4A, 0x53,
    0x5C, 0x68, 0x74, 0x82, 0x91, 0xA3, 0xB6, 0xCC, 0xE5, 0x100,
    0x100, 0xE5, 0xCC, 0xB6, 0xA3, 0x91, 0x82, 0x74, 0x68, 0x5C,
    0x53, 0x4A, 0x42, 0x3B, 0x35, 0x2F, 0x2A, 0x25, 0x21, 0x1E,
    0x1B, 0x18, 0x15, 0x13, 0x11, 0x0F, 0x0E, 0x0C, 0x0B, 0x0A,
    0x09, 0x08, 0x07, 0x06, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03,
    0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00
};

static const uint16_t brightness_map[101] = {
      0U,   3U,   5U,   8U,  10U,  13U,  15U,  18U,  20U,  23U,  26U,  28U,  31U,  33U,  36U,  38U,
     41U,  43U,  46U,  48U,  51U,  54U,  56U,  59U,  61U,  64U,  66U,  69U,  71U,  74U,  77U,  79U,
     82U,  84U,  87U,  89U,  92U,  94U,  97U,  99U, 102U, 105U, 107U, 110U, 112U, 115U, 117U, 120U,
    122U, 125U, 128U, 130U, 133U, 135U, 138U, 140U, 143U, 145U, 148U, 150U, 153U, 156U, 158U, 161U,
    163U, 166U, 168U, 171U, 173U, 176U, 179U, 181U, 184U, 186U, 189U, 191U, 194U, 196U, 199U, 201U,
    204U, 207U, 209U, 212U, 214U, 217U, 219U, 222U, 224U, 227U, 230U, 232U, 235U, 237U, 240U, 242U,
    245U, 247U, 250U, 252U, 256U
};

void ws2812_effect_solid(ws2812_frame_t *frame, uint32_t start_led, uint32_t end_led, uint32_t color, uint16_t bright)
{
    if (bright > 100U) bright = 100U;
    bright = brightness_map[bright];
    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) {
        return;
    }
    uint32_t red = (color >> 16U) & 255U;
    uint32_t green = (color >> 8U) & 255U;
    uint32_t blue = color & 255U;

    color = WS2812_RGB(red * bright >> 8U, green * bright >> 8U, blue * bright >> 8U);

    ws2812_frame_fill(frame, start_led, end_led, color);
}

void ws2812_effect_off(ws2812_frame_t *frame, uint32_t start_led, uint32_t end_led)
{
    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) {
        return;
    }

    ws2812_frame_fill(frame, start_led, end_led, WS2812_RGB(0, 0, 0));
}

void ws2812_effect_breath(ws2812_frame_t *frame, uint32_t start_led, uint32_t end_led, uint32_t color, ws2812_breath_state_t *state)
{
    uint8_t breath_value;
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) {
        return;
    }

    if (state == NULL) return;
    if (state->table_index >= BREATH_MAP_SIZE) state->table_index = 0U;
    breath_value = (uint8_t)breath_map[state->table_index];
    red = ((color >> 16U) & 0xFFU) * breath_value >> 8U;
    green = ((color >> 8U) & 0xFFU) * breath_value >> 8U;
    blue = (color & 0xFFU) * breath_value >> 8U;
    ws2812_frame_fill(frame, start_led, end_led, WS2812_RGB(red, green, blue));

    state->table_index++;
    if (state->table_index >= BREATH_MAP_SIZE) {
        state->table_index = 0U;
    }
}

/* 绘制当前闪烁阶段；同组区间绘制完成后只调用一次 flash_update。 */
void ws2812_effect_flash(ws2812_frame_t *frame, uint32_t start_led, uint32_t end_led, uint32_t color1,
    uint32_t color2, int8_t cycle_limit, uint8_t brightness, const ws2812_flash_state_t *flash_state)
{
    uint32_t run_color;

    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U || flash_state == NULL) {
        return;
    }

    if (cycle_limit >= 0 && flash_state->completed_cycles >= (uint8_t)cycle_limit) {
        ws2812_effect_off(frame, start_led, end_led);
        return;
    }

    if (flash_state->stage == 0U) {
        run_color = color1;
    }
    else {
        run_color = color2;
    }

    ws2812_effect_solid(frame, start_led, end_led, run_color, brightness);
}

void ws2812_flash_update(uint8_t color1_ticks, uint8_t color2_ticks, int8_t cycle_limit, ws2812_flash_state_t *flash_state)
{
    uint8_t duration_ticks;

    if (flash_state == NULL || color1_ticks == 0U || color2_ticks == 0U ||
        (cycle_limit >= 0 && flash_state->completed_cycles >= (uint8_t)cycle_limit)) {
        return;
    }

    duration_ticks = flash_state->stage == 0U ? color1_ticks : color2_ticks;
    flash_state->elapsed_ticks++;
    if (flash_state->elapsed_ticks < duration_ticks) {
        return;
    }

    flash_state->elapsed_ticks = 0U;
    if (flash_state->stage == 0U) {
        flash_state->stage = 1U;
        return;
    }

    flash_state->stage = 0U;
    if (cycle_limit >= 0) {
        flash_state->completed_cycles++;
    }
}

/* 从 head 向后绘制 length 颗灯珠，亮度逐颗递减。 */
void ws2812_effect_trail(ws2812_frame_t *frame, uint32_t color, uint8_t step_ticks, ws2812_trail_state_t *trail_state)
{
    uint32_t led_index = 0U;
    uint8_t bright = 0;
    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U) return;
    if (trail_state == NULL) return;

    uint32_t strip_led_count = frame->led_count;

    if (trail_state->length > strip_led_count) trail_state->length = strip_led_count;

    if (trail_state->head >= strip_led_count) trail_state->head = 0U;

    for (uint32_t i = 0U; i < trail_state->length; i++) {
        bright = WS2812_TRAIL_BRIGHTNESS > i * 10U ? (uint8_t)(WS2812_TRAIL_BRIGHTNESS - i * 10U) : 0U;

        if (trail_state->head >= i)
            led_index = trail_state->head - i;
        else
            led_index = trail_state->head + strip_led_count - i;

        ws2812_effect_solid(frame, led_index, led_index, color, bright);
    }

    if (step_ticks == 0U) {
        step_ticks = 1U;
    }

    trail_state->elapsed_ticks++;
    if (trail_state->elapsed_ticks >= step_ticks) {
        trail_state->elapsed_ticks = 0U;
        trail_state->head++;
    }
}

/* 奇数宽度的周期波浪：中心最亮，向两边衰减，边缘至少 15%。 */
void ws2812_effect_wave(ws2812_frame_t *frame, uint32_t color, uint8_t wave_width, uint8_t step_ticks, ws2812_wave_state_t *wave_state)
{
    uint32_t strip_led_count;
    uint32_t half_wave_width;

    if (frame == NULL || frame->pixels == NULL || frame->led_count == 0U || wave_state == NULL || wave_width < 3U || (wave_width & 1U) == 0U) {
        return;
    }

    strip_led_count = frame->led_count;
    half_wave_width = wave_width >> 1U;
    if (wave_state->head >= wave_width) {
        wave_state->head = 0U;
    }

    for (uint32_t led_index = 0U; led_index < strip_led_count; led_index++) {
        uint32_t wave_phase = led_index + wave_width - wave_state->head;
        uint32_t distance;
        uint32_t attenuation;
        uint8_t brightness;

        while (wave_phase >= wave_width) {
            wave_phase -= wave_width;
        }

        if (wave_phase <= half_wave_width) {
            distance = wave_phase;
        }
        else {
            distance = wave_width - wave_phase;
        }

        attenuation = distance << WS2812_WAVE_BRIGHTNESS_STEP_SHIFT;
        if (attenuation >= (WS2812_TRAIL_BRIGHTNESS - WS2812_WAVE_EDGE_BRIGHTNESS)) {
            brightness = WS2812_WAVE_EDGE_BRIGHTNESS;
        }
        else {
            brightness = WS2812_TRAIL_BRIGHTNESS - attenuation;
        }
        ws2812_effect_solid(frame, led_index, led_index, color, brightness);
    }

    if (step_ticks == 0U) {
        step_ticks = 1U;
    }

    wave_state->elapsed_ticks++;
    if (wave_state->elapsed_ticks >= step_ticks) {
        wave_state->elapsed_ticks = 0U;
        wave_state->head++;
        if (wave_state->head >= wave_width) {
            wave_state->head = 0U;
        }
    }
}

void ws2812_render_basic_effect(ws2812_frame_t *frame, ws2812_basic_effect_e effect, uint32_t elapsed_ms)
{
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t color;

    if (count == 0U) {
        return;
    }
    switch (effect) {
    case WS2812_BASIC_WHITE_TRAILING: {
        ws2812_trail_state_t run = { .head = (elapsed_ms / 100U) % count, .length = 6U, .elapsed_ticks = 0U };

        ws2812_effect_off(frame, 0U, count - 1U);
        ws2812_effect_trail(frame, WS2812_RGB(255U, 255U, 255U), 2U, &run);
        return;
    }
    case WS2812_BASIC_GREEN_FLASH_THEN_STEADY:
        color = elapsed_ms >= 1200U || elapsed_ms % 400U < 200U ? WS2812_RGB(0U, 255U, 0U) : 0U;
        break;
    case WS2812_BASIC_YELLOW_STEADY:
        color = WS2812_RGB(255U, 255U, 0U);
        break;
    case WS2812_BASIC_RED_FLASH:
        color = elapsed_ms % 1000U < 700U ? WS2812_RGB(255U, 0U, 0U) : 0U;
        break;
    case WS2812_BASIC_RED_STEADY:
        color = WS2812_RGB(255U, 0U, 0U);
        break;
    case WS2812_BASIC_WHITE_BREATHING: {
        uint32_t phase = elapsed_ms % 4000U;
        uint32_t level = 255U * (phase <= 2000U ? phase : 4000U - phase) / 2000U;

        color = WS2812_RGB(level, level, level);
        break;
    }
    default:
        return;
    }
    ws2812_frame_fill(frame, 0U, count - 1U, color);
}
