#ifndef _MODULE_WS2812_EFFECT_H_
#define _MODULE_WS2812_EFFECT_H_

#include "module_ws2812.h"

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



    void module_ws2812_effect_at(module_ws2812_id_e strip_id,
                                 uint32_t start_led,
                                 uint32_t end_led,
                                 uint32_t color,
                                 uint16_t bright);
    void module_ws2812_effect_off(module_ws2812_id_e strip_id,
                                  uint32_t start_led,
                                  uint32_t end_led);
    void module_ws2812_effect_breath(module_ws2812_id_e strip_id,
                                     uint32_t start_led,
                                     uint32_t end_led,
                                     uint32_t color);
void ws2812_flash_with(module_ws2812_id_e strip_id,
                       uint32_t start_led,
                       uint32_t end_led,
                       uint32_t color1,
                       uint32_t color2,
                       int8_t cycle_limit,
                       uint8_t brightness,
                       const strip_flash_state_t *flash_state);
void ws2812_flash_update(uint8_t color1_ticks,
                          uint8_t color2_ticks,
                          int8_t cycle_limit,
                          strip_flash_state_t *flash_state);
void ws2812_run_with(module_ws2812_id_e strip_id, uint32_t color, uint8_t hz, strip_run_state_t* strip_run_state);
void ws2812_wave_with(module_ws2812_id_e strip_id,
                      uint32_t color,
                      uint8_t wave_width,
                      uint8_t hz,
                      strip_wave_state_t *wave_state);
#ifdef __cplusplus
}
#endif

#endif /* MODULE_WS2812_EFFECT_H */
