#include "module_ws2812_effect.h"

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

void module_ws2812_effect_at(module_ws2812_id_e strip_id,
                             uint32_t start_led,
                             uint32_t end_led,
                             uint32_t color,
                             uint16_t bright)
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

void module_ws2812_effect_off(module_ws2812_id_e strip_id,
                              uint32_t start_led,
                              uint32_t end_led)
{
    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT) {
        return;
    }

    module_ws2812_set_rgb_range(strip_id, start_led, end_led, WS2812_RGB(0, 0, 0));
}

void module_ws2812_effect_breath(module_ws2812_id_e strip_id,
                                 uint32_t start_led,
                                 uint32_t end_led,
                                 uint32_t color)
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
void ws2812_flash_with(module_ws2812_id_e strip_id,
                       uint32_t start_led, uint32_t end_led,
                       uint32_t color1, uint32_t color2,
                       int8_t cycle_limit, uint8_t brightness,
                       const strip_flash_state_t *flash_state)
{
    uint32_t run_color;

    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT ||
        flash_state == NULL)
    {
        return;
    }

    if (cycle_limit >= 0 &&
        flash_state->completed_cycles >= (uint8_t)cycle_limit)
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

void ws2812_flash_update(uint8_t color1_ticks,
                          uint8_t color2_ticks,
                          int8_t cycle_limit,
                          strip_flash_state_t *flash_state)
{
    uint8_t duration_ticks;

    if (flash_state == NULL ||
        color1_ticks == 0U ||
        color2_ticks == 0U ||
        (cycle_limit >= 0 &&
         flash_state->completed_cycles >= (uint8_t)cycle_limit))
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
void ws2812_wave_with(module_ws2812_id_e strip_id,
                      uint32_t color,
                      uint8_t wave_width,
                      uint8_t hz,
                      strip_wave_state_t *wave_state)
{
    uint32_t strip_led_count;
    uint32_t half_wave_width;

    if ((uint32_t)strip_id >= (uint32_t)MODULE_WS2812_ID_COUNT ||
        wave_state == NULL ||
        wave_width < 3U ||
        (wave_width & 1U) == 0U)
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
