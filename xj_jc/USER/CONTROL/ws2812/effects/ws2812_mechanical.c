#include "CONTROL/ws2812/effects/ws2812_mechanical.h"
#include "CONTROL/ws2812/ws2812_frame.h"

#include <stddef.h>

/* 固定计算尺度，不是灯珠数量或调光参数。位置按实际 led_count 映射。
 * 环形效果：一圈为 POSITION_SPAN；中心扩散：中心到端点为 POSITION_SPAN。 */
enum { RGB_MAX = 255U, POSITION_SPAN = 256U, RING_HALF_SPAN = POSITION_SPAN / 2U };

/* 距离包络：中心 255，边缘 0，平方衰减形成柔和余辉。 */
static uint32_t ws2812_glow(int32_t distance, uint32_t width)
{
    uint32_t magnitude = (uint32_t)(distance < 0 ? -distance : distance);
    uint32_t level;

    if (magnitude >= width) {
        return 0U;
    }
    level = RGB_MAX * (width - magnitude) / width;
    return level * level / RGB_MAX;
}

static uint32_t ws2812_scale_color(uint32_t color, uint32_t level)
{
    uint32_t red = ((color >> 16U) & RGB_MAX) * level / RGB_MAX;
    uint32_t green = ((color >> 8U) & RGB_MAX) * level / RGB_MAX;
    uint32_t blue = (color & RGB_MAX) * level / RGB_MAX;

    return WS2812_RGB(red, green, blue);
}

/* 光层相加时饱和到 255，避免溢出后突然变暗或串色。 */
static uint32_t ws2812_add_color(uint32_t first, uint32_t second)
{
    uint32_t red = ((first >> 16U) & RGB_MAX) + ((second >> 16U) & RGB_MAX);
    uint32_t green = ((first >> 8U) & RGB_MAX) + ((second >> 8U) & RGB_MAX);
    uint32_t blue = (first & RGB_MAX) + (second & RGB_MAX);

    return WS2812_RGB(red > RGB_MAX ? RGB_MAX : red, green > RGB_MAX ? RGB_MAX : green, blue > RGB_MAX ? RGB_MAX : blue);
}

/* 中心距离归一化到 0..256；偶数灯珠的中间两颗等距。 */
static uint32_t ws2812_center_distance(uint32_t index, uint32_t count)
{
    uint32_t nearest_end = index < count - 1U - index ? index : count - 1U - index;
    uint32_t half_span = (count - 1U) / 2U;

    return half_span > 0U ? (half_span - nearest_end) * POSITION_SPAN / half_span : 0U;
}

void ws2812_effect_reactor_heartbeat(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        cycle_ms = 3600U,
        first_peak_ms = 350U, first_half_width_ms = 350U,
        second_peak_ms = 1000U, second_half_width_ms = 500U,
        second_gain_numerator = 2U, second_gain_denominator = 3U,
        edge_delay_ms = 128U, edge_level_percent = 20U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 3U, 9U);
    const uint32_t pulse_color = WS2812_RGB(140U, 225U, RGB_MAX);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t phase = elapsed_ms % cycle_ms;

    /* 一强一弱双脉冲，向外略微延迟；其余时间保持深蓝待机辉光。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        int32_t local_phase = (int32_t)phase - (int32_t)(distance * edge_delay_ms / POSITION_SPAN);
        uint32_t pulse = ws2812_glow(local_phase - first_peak_ms, first_half_width_ms) +
            ws2812_glow(local_phase - second_peak_ms, second_half_width_ms) * second_gain_numerator / second_gain_denominator;
        uint32_t level = pulse * (POSITION_SPAN * 100U - distance * (100U - edge_level_percent)) / (POSITION_SPAN * 100U);
        uint32_t color = ws2812_add_color(idle_color, ws2812_scale_color(pulse_color, level));

        ws2812_frame_fill(frame, index, index, color);
    }
}

void ws2812_effect_dual_rotor(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        rotation_ms = 3200U,
        /* 宽度单位为归一化位置：112/256 圈拖尾，24/256 圈头部柔化。 */
        tail_width_pos = 112U, head_width_pos = 24U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 2U, 5U);
    const uint32_t forward_color = WS2812_RGB(30U, 190U, RGB_MAX);
    const uint32_t reverse_color = WS2812_RGB(235U, 85U, 8U);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t head = (elapsed_ms % rotation_ms) * POSITION_SPAN / rotation_ms;
    uint32_t reverse_head = (POSITION_SPAN + RING_HALF_SPAN - head) % POSITION_SPAN;

    /* 冰蓝与琥珀转子反向运行，重叠时叠加光强；每圈 3.2 秒。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t position = index * POSITION_SPAN / count;
        uint32_t forward_distance = (head + POSITION_SPAN - position) % POSITION_SPAN;
        uint32_t reverse_distance = (position + POSITION_SPAN - reverse_head) % POSITION_SPAN;
        uint32_t forward_level = forward_distance < RING_HALF_SPAN ? ws2812_glow((int32_t)forward_distance, tail_width_pos) : ws2812_glow((int32_t)(POSITION_SPAN - forward_distance), head_width_pos);
        uint32_t reverse_level = reverse_distance < RING_HALF_SPAN ? ws2812_glow((int32_t)reverse_distance, tail_width_pos) : ws2812_glow((int32_t)(POSITION_SPAN - reverse_distance), head_width_pos);
        uint32_t color = ws2812_add_color(ws2812_scale_color(forward_color, forward_level),
            ws2812_scale_color(reverse_color, reverse_level));

        ws2812_frame_fill(frame, index, index, ws2812_add_color(color, idle_color));
    }
}

void ws2812_effect_heavy_scan(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        traverse_ms = 1800U, endpoint_hold_ms = 600U,
        half_cycle_ms = traverse_ms + endpoint_hold_ms,
        cycle_ms = 2U * half_cycle_ms,
        /* 光晕/核心的半宽分别占整条线性灯带的 100/256、28/256。 */
        halo_width_pos = 100U, core_width_pos = 28U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 2U, 5U);
    const uint32_t halo_color = WS2812_RGB(180U, 48U, 3U);
    const uint32_t core_color = WS2812_RGB(110U, 150U, 150U);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t phase = elapsed_ms % cycle_ms;
    uint32_t travel = phase <= half_cycle_ms ? phase : cycle_ms - phase;
    uint32_t position = travel <= endpoint_hold_ms / 2U ? 0U :
        (travel >= half_cycle_ms - endpoint_hold_ms / 2U ? POSITION_SPAN : (travel - endpoint_hold_ms / 2U) * POSITION_SPAN / traverse_ms);

    /* smoothstep：x²(3-2x)，位置尺度为 POSITION_SPAN；端点停顿分到折返点两侧。 */
    position = position * position * (3U * POSITION_SPAN - 2U * position) / (POSITION_SPAN * POSITION_SPAN);
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t led_position = count > 1U ? (index * POSITION_SPAN + (count - 1U) / 2U) / (count - 1U) : 0U;
        int32_t distance = (int32_t)led_position - (int32_t)position;
        uint32_t color = ws2812_scale_color(halo_color, ws2812_glow(distance, halo_width_pos));

        color = ws2812_add_color(color, ws2812_scale_color(core_color, ws2812_glow(distance, core_width_pos)));
        ws2812_frame_fill(frame, index, index, ws2812_add_color(color, idle_color));
    }
}

void ws2812_effect_shockwave(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        cycle_ms = 2800U, ignition_ms = 400U,
        /* 1800 ms 传播 400 个位置单位，超过端点 256，让余波完整移出灯带。 */
        propagation_ms = 1800U, propagation_distance_pos = 400U,
        wave_width_pos = 90U, wake_delay_pos = 110U, wake_width_pos = 120U,
        ignition_width_pos = 110U, wake_gain_divisor = 3U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 1U, 4U);
    const uint32_t wave_color = WS2812_RGB(160U, 225U, RGB_MAX);
    const uint32_t wake_color = WS2812_RGB(0U, 160U, 180U);
    const uint32_t ignition_color = WS2812_RGB(230U, 65U, 3U);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t phase = elapsed_ms % cycle_ms;
    uint32_t radius = phase * propagation_distance_pos / propagation_ms;

    /* 中央琥珀点火，冷白冲击波向两端扩散，随后留下青色余波。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        uint32_t wave = ws2812_glow((int32_t)distance - (int32_t)radius, wave_width_pos);
        uint32_t wake = ws2812_glow((int32_t)distance - (int32_t)radius + wake_delay_pos, wake_width_pos) / wake_gain_divisor;
        uint32_t ignition = ws2812_glow((int32_t)distance, ignition_width_pos) * (phase < ignition_ms ? ignition_ms - phase : 0U) / ignition_ms;
        uint32_t color = ws2812_add_color(ws2812_scale_color(wave_color, wave),
            ws2812_scale_color(wake_color, wake));

        color = ws2812_add_color(color, ws2812_scale_color(ignition_color, ignition));
        ws2812_frame_fill(frame, index, index, ws2812_add_color(color, idle_color));
    }
}

void ws2812_effect_cold_plasma(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        first_rotation_ms = 7200U, second_rotation_ms = 10800U,
        /* 此效果以 512 单位表示一圈，使光带中心两侧各有 256 单位。 */
        wave_span_pos = 2U * POSITION_SPAN,
        first_width_pos = POSITION_SPAN, second_width_pos = 220U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 3U, 12U);
    const uint32_t first_color = WS2812_RGB(4U, 70U, 170U);
    const uint32_t second_color = WS2812_RGB(12U, 130U, 105U);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t first_phase = (elapsed_ms % first_rotation_ms) * wave_span_pos / first_rotation_ms;
    uint32_t second_phase = (elapsed_ms % second_rotation_ms) * wave_span_pos / second_rotation_ms;

    /* 两层不同速度、相反方向的蓝青光带，缓慢交叠形成等离子流。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t position = index * wave_span_pos / count;
        uint32_t first_level = ws2812_glow((int32_t)((position + first_phase) % wave_span_pos) - POSITION_SPAN, first_width_pos);
        uint32_t second_level = ws2812_glow((int32_t)((position + wave_span_pos - second_phase) % wave_span_pos) - POSITION_SPAN, second_width_pos);
        uint32_t color = ws2812_add_color(ws2812_scale_color(first_color, first_level),
            ws2812_scale_color(second_color, second_level));

        ws2812_frame_fill(frame, index, index, ws2812_add_color(color, idle_color));
    }
}

void ws2812_effect_warp_charge(ws2812_frame_t *frame, uint32_t elapsed_ms)
{
    enum {
        charge_ms = 3000U, lock_ms = 400U, release_ms = 200U, cooldown_ms = 1400U, idle_ms = 1000U,
        lock_end_ms = charge_ms + lock_ms,
        release_end_ms = lock_end_ms + release_ms,
        cooldown_end_ms = release_end_ms + cooldown_ms,
        cycle_ms = cooldown_end_ms + idle_ms,
        front_width_pos = 55U, charge_max_level = 160U,
    };
    const uint32_t idle_color = WS2812_RGB(0U, 2U, 6U);
    const uint32_t charge_color = WS2812_RGB(10U, 130U, 230U);
    const uint32_t front_color = WS2812_RGB(110U, 200U, 230U);
    const uint32_t lock_color = WS2812_RGB(75U, 105U, 125U);
    const uint32_t release_color = WS2812_RGB(200U, 235U, RGB_MAX);
    const uint32_t cooldown_color = WS2812_RGB(140U, 200U, 245U);
    uint32_t count = frame != NULL && frame->pixels != NULL ? frame->led_count : 0U;
    uint32_t phase = elapsed_ms % cycle_ms;

    /* 阶段终点从持续时间推导，调整某阶段时不必手动同步后续时间。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        uint32_t color = idle_color;

        if (phase < charge_ms) {
            int32_t front = POSITION_SPAN - (int32_t)(phase * POSITION_SPAN / charge_ms);
            int32_t behind = (int32_t)distance - front;
            uint32_t charge = behind > 0 ? (uint32_t)behind * charge_max_level / POSITION_SPAN : 0U;

            color = ws2812_add_color(color, ws2812_scale_color(charge_color, charge));
            color = ws2812_add_color(color, ws2812_scale_color(front_color, ws2812_glow(behind, front_width_pos)));
        } else if (phase < lock_end_ms) {
            color = lock_color;
        } else if (phase < release_end_ms) {
            color = release_color;
        } else if (phase < cooldown_end_ms) {
            uint32_t remaining = cooldown_end_ms - phase;
            uint32_t level = remaining * remaining * RGB_MAX / (cooldown_ms * cooldown_ms);

            color = ws2812_add_color(color, ws2812_scale_color(cooldown_color, level));
        }
        ws2812_frame_fill(frame, index, index, color);
    }
}
