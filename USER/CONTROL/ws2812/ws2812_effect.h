#ifndef WS2812_EFFECT_H
#define WS2812_EFFECT_H

#include "CONTROL/ws2812/ws2812_driver.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct {
        uint8_t stage;
        uint8_t elapsed_ticks;
        uint8_t completed_cycles;
    } strip_flash_state_t;


    typedef struct
    {
        uint8_t idx;
        uint8_t len;
        uint8_t hz;
    }strip_run_state_t;

    typedef struct
    {
        uint8_t idx;
        uint8_t hz_ticks;
    } strip_wave_state_t;


    void module_ws2812_effect_at(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color, uint16_t bright);
    void module_ws2812_effect_off(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led);
    void module_ws2812_effect_breath(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color);
void ws2812_flash_with(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color1,
    uint32_t color2, int8_t cycle_limit, uint8_t brightness, const strip_flash_state_t *flash_state);
void ws2812_flash_update(uint8_t color1_ticks, uint8_t color2_ticks, int8_t cycle_limit, strip_flash_state_t *flash_state);
void ws2812_run_with(module_ws2812_id_e strip_id, uint32_t color, uint8_t hz, strip_run_state_t* strip_run_state);
void ws2812_wave_with(module_ws2812_id_e strip_id, uint32_t color, uint8_t wave_width, uint8_t hz, strip_wave_state_t *wave_state);

/* 原六种调试灯效保留为基础灯效库，不依赖 DEBUG_ENABLE。 */
typedef enum {
    WS2812_BASIC_WHITE_TRAILING = 1,
    WS2812_BASIC_GREEN_FLASH_THEN_STEADY,
    WS2812_BASIC_YELLOW_STEADY,
    WS2812_BASIC_RED_FLASH,
    WS2812_BASIC_RED_STEADY,
    WS2812_BASIC_WHITE_BREATHING,
} ws2812_basic_effect_e;

/* 以下接口只绘制工作帧，不启动发送；elapsed_ms 为本次播放已过时间，传 0 从头播放。 */
void ws2812_render_basic_effect(module_ws2812_id_e strip_id, ws2812_basic_effect_e effect, uint32_t elapsed_ms);
void ws2812_effect_reactor_heartbeat(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
void ws2812_effect_dual_rotor(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
void ws2812_effect_heavy_scan(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
void ws2812_effect_shockwave(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
void ws2812_effect_cold_plasma(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
void ws2812_effect_warp_charge(module_ws2812_id_e strip_id, uint32_t elapsed_ms);
#ifdef __cplusplus
}
#endif

#endif /* MODULE_WS2812_EFFECT_H */
