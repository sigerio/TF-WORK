#ifndef MODULE_WS2812_STATUS_H
#define MODULE_WS2812_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define MODULE_WS2812_PERIOD_MS 50

/* 灯带使能脚（PC5，拉低触发）：注释掉则一直使能 */
#define MODULE_WS2812_STRIP_ENABLE_PIN 1

typedef enum{
    E_DOOR_RUN_IDLE = 0,
    E_DOOR_RUN_IN_ACTION,
}door_running_in_e;

typedef enum{
    E_PLATFORM_RUN_IDLE = 0,
    E_PLATFORM_RUN_IN_ACTION,
    E_PLATFORM_NAVIGATE,
}platform_running_in_e;

void set_door_running_in(door_running_in_e in);
void set_platform_running_in(platform_running_in_e in);
void module_ws2812_status_update(uint16_t);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_WS2812_STATUS_H */
