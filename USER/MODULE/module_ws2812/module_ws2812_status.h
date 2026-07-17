#ifndef MODULE_WS2812_STATUS_H
#define MODULE_WS2812_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define MODULE_WS2812_PERIOD_MS 50


typedef enum{
    E_DOOR_RUN_IDLE = 0,
    E_DOOR_RUN_IN_ACTION,
}door_running_in_e;

typedef enum{
    E_HELIPED_RUN_IDLE = 0,
    E_HELIPED_RUN_IN_ACTION,
    E_HELIPED_NAVIGATE,
}heliped_running_in_e;

void set_door_running_in(door_running_in_e in);
void set_heliped_running_in(heliped_running_in_e in);
void module_ws2812_status_update(uint16_t);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_WS2812_STATUS_H */
