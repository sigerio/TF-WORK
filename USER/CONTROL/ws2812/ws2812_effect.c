#include "CONTROL/ws2812/ws2812_effect.h"

#define BREATH_MAP_SIZE 100U
#define WS2812_WAVE_EDGE_BRIGHTNESS 15U
#define WS2812_WAVE_BRIGHTNESS_STEP_SHIFT 4U

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

static uint8_t breath_map_indexes[MODULE_WS2812_ID_COUNT];

void module_ws2812_effect_at(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color, uint16_t bright)
{
    if(bright > 100)    bright = 100;
    bright = brightness_map[bright];
    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT) {
        return;
    }
    uint32_t r = (color&0x00FF0000)>>16;
    uint32_t g = (color&0x0000FF00)>>8;
    uint32_t b = (color&0x000000FF);

    color = ((r*bright>>8)<<16)|((g*bright>>8)<<8)|(b*bright>>8);

    module_ws2812_set_rgb_range(strip_id, start_led, end_led, color);
}

void module_ws2812_effect_off(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led)
{
    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT) {
        return;
    }

    module_ws2812_set_rgb_range(strip_id, start_led, end_led, WS2812_RGB(0, 0, 0));
}

void module_ws2812_effect_breath(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color)
{
    uint8_t breath_value;
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT) {
        return;
    }

    breath_value = breath_map[breath_map_indexes[strip_id]];
    red = ((color >> 16U) & 0xFFU) * breath_value >>8;
    green = ((color >> 8U) & 0xFFU) * breath_value >>8;
    blue = (color & 0xFFU) * breath_value >>8;
    module_ws2812_set_rgb_range(strip_id, start_led, end_led, WS2812_RGB(red, green, blue));

    breath_map_indexes[strip_id]++;
    if (breath_map_indexes[strip_id] >= BREATH_MAP_SIZE) {
        breath_map_indexes[strip_id] = 0U;
    }
}


static uint8_t g_bright = 80;

/*本文件内的灯光效果仅提供底层逻辑，调速调频等功能请于应用时手动调参 work帧会在上层被清空，这里只负责灯效处理*/
/*闪烁 某灯带的[start_led,end_led] color1在c1时间之后，切换为color2，c2时间之后切换为c1*/
void ws2812_flash_with(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color1,
    uint32_t color2, int8_t cycle_limit, uint8_t brightness, const strip_flash_state_t *flash_state)
{
    uint32_t run_color;

    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT || flash_state == NULL)
    {
        return;
    }

    if (cycle_limit >= 0 && flash_state->completed_cycles >= (uint8_t)cycle_limit)
    {
        module_ws2812_effect_off(strip_id, start_led, end_led);
        return;
    }

    if (flash_state->stage == 0U)
    {
        run_color = color1;
    }
    else
    {
        run_color = color2;
    }

    module_ws2812_effect_at(strip_id, start_led, end_led, run_color, brightness);
}

void ws2812_flash_update(uint8_t color1_ticks, uint8_t color2_ticks, int8_t cycle_limit, strip_flash_state_t *flash_state)
{
    uint8_t duration_ticks;

    if (flash_state == NULL || color1_ticks == 0U || color2_ticks == 0U ||
        (cycle_limit >= 0 && flash_state->completed_cycles >= (uint8_t)cycle_limit))
    {
        return;
    }

    duration_ticks = flash_state->stage == 0U ? color1_ticks : color2_ticks;
    flash_state->elapsed_ticks++;
    if (flash_state->elapsed_ticks < duration_ticks)
    {
        return;
    }

    flash_state->elapsed_ticks = 0U;
    if (flash_state->stage == 0U)
    {
        flash_state->stage = 1U;
        return;
    }

    flash_state->stage = 0U;
    if (cycle_limit >= 0)
    {
        flash_state->completed_cycles++;
    }
}


/*单点跑马灯 idx是当前的head， len是指当前跟随几个灯进行，然后呈阶梯减弱(拖尾效果)*/
void ws2812_run_with(module_ws2812_id_e strip_id, uint32_t color, uint8_t hz, strip_run_state_t* strip_run_state)
{
    int idx = 0;
    uint8_t bright = 0;
    if(strip_id >= MODULE_WS2812_ID_COUNT)  return;
    if(NULL == strip_run_state) return;

    uint32_t strip_led_count = get_strip_led_count(strip_id);

    if(strip_run_state->len > strip_led_count)    strip_run_state->len = strip_led_count;


    if(strip_run_state->idx >= strip_led_count) strip_run_state->idx = 0;

    for(int i= 0; i<strip_run_state->len; i++)
    {
        bright = (g_bright>(i*10))?(g_bright - (i*10)):0;

        if(strip_run_state->idx >= i)
            idx = strip_run_state->idx - i;
        else
            idx = strip_run_state->idx + strip_led_count -i;


        module_ws2812_effect_at(strip_id, idx, idx, color, bright);
    }

    if (hz == 0U)
    {
        hz = 1U;
    }

    strip_run_state->hz++;
    if (strip_run_state->hz >= hz)
    {
        strip_run_state->hz = 0U;
        strip_run_state->idx++;
    }
}


/*
波浪效果： 灯珠数目均分成 n个奇数灯珠的区域，区域中心灯珠亮度最亮，向区域两个边缘递减，边缘调整在15%以上
确定一个区域中心灯珠索引idx，其他区域的中心灯珠可以通过计算得到，每次调用idx++，由此可得波浪效果
*/
void ws2812_wave_with(module_ws2812_id_e strip_id, uint32_t color, uint8_t wave_width, uint8_t hz, strip_wave_state_t *wave_state)
{
    uint32_t strip_led_count;
    uint32_t half_wave_width;

    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT || wave_state == NULL || wave_width < 3U || (wave_width & 1U) == 0U)
    {
        return;
    }

    strip_led_count = get_strip_led_count(strip_id);
    half_wave_width = wave_width>>1;
    if (wave_state->idx >= wave_width)
    {
        wave_state->idx = 0U;
    }

    for (uint32_t led_index = 0U; led_index < strip_led_count; led_index++)
    {
        uint32_t wave_phase = led_index + wave_width - wave_state->idx;
        uint32_t distance;
        uint32_t attenuation;
        uint8_t brightness;

        while (wave_phase >= wave_width)
        {
            wave_phase -= wave_width;
        }

        if (wave_phase <= half_wave_width)
        {
            distance = wave_phase;
        }
        else
        {
            distance = wave_width - wave_phase;
        }

        attenuation = distance << WS2812_WAVE_BRIGHTNESS_STEP_SHIFT;
        if (attenuation >= (g_bright - WS2812_WAVE_EDGE_BRIGHTNESS))
        {
            brightness = WS2812_WAVE_EDGE_BRIGHTNESS;
        }
        else
        {
            brightness = g_bright - attenuation;
        }
        module_ws2812_effect_at(strip_id, led_index, led_index, color, brightness);
    }

    if (hz == 0U)
    {
        hz = 1U;
    }

    wave_state->hz_ticks++;
    if (wave_state->hz_ticks >= hz)
    {
        wave_state->hz_ticks = 0U;
        wave_state->idx++;
        if (wave_state->idx >= wave_width)
        {
            wave_state->idx = 0U;
        }
    }
}

/* 全条灯效使用独立播放时间，不占用业务闪烁/跑马状态。 */
static uint32_t ws2812_effect_led_count(module_ws2812_id_e strip_id)
{
    return (uint32_t)strip_id < MODULE_WS2812_ID_COUNT ? get_strip_led_count(strip_id) : 0U;
}

void ws2812_render_basic_effect(module_ws2812_id_e strip_id, ws2812_basic_effect_e effect, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t color;

    if (count == 0U) {
        return;
    }
    switch (effect) {
    case WS2812_BASIC_WHITE_TRAILING: {
        strip_run_state_t run = { .idx = (uint8_t)((elapsed_ms / 100U) % count), .len = 6U, .hz = 0U };

        module_ws2812_effect_off(strip_id, 0U, count - 1U);
        ws2812_run_with(strip_id, WS2812_RGB(255U, 255U, 255U), 2U, &run);
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
    module_ws2812_set_rgb_range(strip_id, 0U, count - 1U, color);
}

/* 距离包络：中心 255，边缘 0，平方衰减形成柔和余辉。 */
static uint32_t ws2812_glow(int32_t distance, uint32_t width)
{
    uint32_t magnitude = (uint32_t)(distance < 0 ? -distance : distance);
    uint32_t level;

    if (magnitude >= width) {
        return 0U;
    }
    level = 255U * (width - magnitude) / width;
    return level * level / 255U;
}

static uint32_t ws2812_scale_color(uint32_t color, uint32_t level)
{
    uint32_t red = ((color >> 16U) & 255U) * level / 255U;
    uint32_t green = ((color >> 8U) & 255U) * level / 255U;
    uint32_t blue = (color & 255U) * level / 255U;

    return WS2812_RGB(red, green, blue);
}

/* 光层相加时饱和到 255，避免溢出后突然变暗或串色。 */
static uint32_t ws2812_add_color(uint32_t first, uint32_t second)
{
    uint32_t red = ((first >> 16U) & 255U) + ((second >> 16U) & 255U);
    uint32_t green = ((first >> 8U) & 255U) + ((second >> 8U) & 255U);
    uint32_t blue = (first & 255U) + (second & 255U);

    return WS2812_RGB(red > 255U ? 255U : red, green > 255U ? 255U : green, blue > 255U ? 255U : blue);
}

/* 中心距离归一化到 0..256；偶数灯珠的中间两颗等距。 */
static uint32_t ws2812_center_distance(uint32_t index, uint32_t count)
{
    uint32_t nearest_end = index < count - 1U - index ? index : count - 1U - index;
    uint32_t half_span = (count - 1U) / 2U;

    return half_span > 0U ? (half_span - nearest_end) * 256U / half_span : 0U;
}

void ws2812_effect_reactor_heartbeat(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t phase = elapsed_ms % 3600U;

    /* 一强一弱双脉冲，向外略微延迟；其余时间保持深蓝待机辉光。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        int32_t local_phase = (int32_t)phase - (int32_t)(distance / 2U);
        uint32_t pulse = ws2812_glow(local_phase - 350, 350U) + ws2812_glow(local_phase - 1000, 500U) * 2U / 3U;
        uint32_t level = pulse * (320U - distance) / 320U;
        uint32_t color = ws2812_add_color(WS2812_RGB(0U, 3U, 9U), ws2812_scale_color(WS2812_RGB(140U, 225U, 255U), level));

        module_ws2812_set_rgb_range(strip_id, index, index, color);
    }
}

void ws2812_effect_dual_rotor(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t head = (elapsed_ms % 3200U) * 256U / 3200U;
    uint32_t reverse_head = (384U - head) % 256U;

    /* 冰蓝与琥珀转子反向运行，重叠时叠加光强；每圈 3.2 秒。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t position = index * 256U / count;
        uint32_t forward_distance = (head + 256U - position) % 256U;
        uint32_t reverse_distance = (position + 256U - reverse_head) % 256U;
        uint32_t forward_level = forward_distance < 128U ? ws2812_glow((int32_t)forward_distance, 112U) : ws2812_glow((int32_t)(256U - forward_distance), 24U);
        uint32_t reverse_level = reverse_distance < 128U ? ws2812_glow((int32_t)reverse_distance, 112U) : ws2812_glow((int32_t)(256U - reverse_distance), 24U);
        uint32_t color = ws2812_add_color(ws2812_scale_color(WS2812_RGB(30U, 190U, 255U), forward_level),
            ws2812_scale_color(WS2812_RGB(235U, 85U, 8U), reverse_level));

        module_ws2812_set_rgb_range(strip_id, index, index, ws2812_add_color(color, WS2812_RGB(0U, 2U, 5U)));
    }
}

void ws2812_effect_heavy_scan(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t phase = elapsed_ms % 4800U;
    uint32_t travel = phase <= 2400U ? phase : 4800U - phase;
    uint32_t position = travel <= 300U ? 0U : (travel >= 2100U ? 256U : (travel - 300U) * 256U / 1800U);

    /* 平滑起停；两端各停留 600 ms，形成重型滑轨的往复节奏。 */
    position = position * position * (768U - 2U * position) / 65536U;
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t led_position = count > 1U ? (index * 256U + (count - 1U) / 2U) / (count - 1U) : 0U;
        int32_t distance = (int32_t)led_position - (int32_t)position;
        uint32_t color = ws2812_scale_color(WS2812_RGB(180U, 48U, 3U), ws2812_glow(distance, 100U));

        color = ws2812_add_color(color, ws2812_scale_color(WS2812_RGB(110U, 150U, 150U), ws2812_glow(distance, 28U)));
        module_ws2812_set_rgb_range(strip_id, index, index, ws2812_add_color(color, WS2812_RGB(0U, 2U, 5U)));
    }
}

void ws2812_effect_shockwave(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t phase = elapsed_ms % 2800U;
    uint32_t radius = phase * 400U / 1800U;

    /* 中央琥珀点火，冷白冲击波向两端扩散，随后留下青色余波。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        uint32_t wave = ws2812_glow((int32_t)distance - (int32_t)radius, 90U);
        uint32_t wake = ws2812_glow((int32_t)distance - (int32_t)radius + 110, 120U) / 3U;
        uint32_t ignition = ws2812_glow((int32_t)distance, 110U) * (phase < 400U ? 400U - phase : 0U) / 400U;
        uint32_t color = ws2812_add_color(ws2812_scale_color(WS2812_RGB(160U, 225U, 255U), wave),
            ws2812_scale_color(WS2812_RGB(0U, 160U, 180U), wake));

        color = ws2812_add_color(color, ws2812_scale_color(WS2812_RGB(230U, 65U, 3U), ignition));
        module_ws2812_set_rgb_range(strip_id, index, index, ws2812_add_color(color, WS2812_RGB(0U, 1U, 4U)));
    }
}

void ws2812_effect_cold_plasma(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t first_phase = (elapsed_ms % 7200U) * 512U / 7200U;
    uint32_t second_phase = (elapsed_ms % 10800U) * 512U / 10800U;

    /* 两层不同速度、相反方向的蓝青光带，缓慢交叠形成等离子流。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t position = index * 512U / count;
        uint32_t first_level = ws2812_glow((int32_t)((position + first_phase) % 512U) - 256, 256U);
        uint32_t second_level = ws2812_glow((int32_t)((position + 512U - second_phase) % 512U) - 256, 220U);
        uint32_t color = ws2812_add_color(ws2812_scale_color(WS2812_RGB(4U, 70U, 170U), first_level),
            ws2812_scale_color(WS2812_RGB(12U, 130U, 105U), second_level));

        module_ws2812_set_rgb_range(strip_id, index, index, ws2812_add_color(color, WS2812_RGB(0U, 3U, 12U)));
    }
}

void ws2812_effect_warp_charge(module_ws2812_id_e strip_id, uint32_t elapsed_ms)
{
    uint32_t count = ws2812_effect_led_count(strip_id);
    uint32_t phase = elapsed_ms % 6000U;

    /* 3 秒由外向内蓄能、400 ms 锁定、200 ms 冷白释放、1.4 秒冷却、1 秒待机。 */
    for (uint32_t index = 0U; index < count; index++) {
        uint32_t distance = ws2812_center_distance(index, count);
        uint32_t color = WS2812_RGB(0U, 2U, 6U);

        if (phase < 3000U) {
            int32_t front = 256 - (int32_t)(phase * 256U / 3000U);
            int32_t behind = (int32_t)distance - front;
            uint32_t charge = behind > 0 ? (uint32_t)behind * 160U / 256U : 0U;

            color = ws2812_add_color(color, ws2812_scale_color(WS2812_RGB(10U, 130U, 230U), charge));
            color = ws2812_add_color(color, ws2812_scale_color(WS2812_RGB(110U, 200U, 230U), ws2812_glow(behind, 55U)));
        } else if (phase < 3400U) {
            color = WS2812_RGB(75U, 105U, 125U);
        } else if (phase < 3600U) {
            color = WS2812_RGB(200U, 235U, 255U);
        } else if (phase < 5000U) {
            uint32_t remaining = 5000U - phase;
            uint32_t level = remaining * remaining * 255U / (1400U * 1400U);

            color = ws2812_add_color(color, ws2812_scale_color(WS2812_RGB(140U, 200U, 245U), level));
        }
        module_ws2812_set_rgb_range(strip_id, index, index, color);
    }
}
