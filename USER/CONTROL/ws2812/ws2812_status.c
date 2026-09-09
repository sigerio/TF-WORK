#include "CONTROL/ws2812/ws2812_status.h"

#include <stdbool.h>

#include "DEBUG/debug_log.h"
#include "CONTROL/ws2812/ws2812_effect.h"

/* SPI0/PA7 的 WS2812 灯效按机库业务状态驱动。 */

static strip_flash_state_t door_flash_state = {0};
static strip_run_state_t platform_run_state = {0};

#ifdef DEBUG_ENABLE
static uint32_t debug_elapsed_ms;

static void ws2812_render_debug_effect(ws2812_debug_effect_e effect)
{
    switch (effect) {
    case WS2812_DEBUG_REACTOR_HEARTBEAT:
        ws2812_effect_reactor_heartbeat(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    case WS2812_DEBUG_DUAL_ROTOR:
        ws2812_effect_dual_rotor(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    case WS2812_DEBUG_HEAVY_SCAN:
        ws2812_effect_heavy_scan(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    case WS2812_DEBUG_SHOCKWAVE:
        ws2812_effect_shockwave(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    case WS2812_DEBUG_COLD_PLASMA:
        ws2812_effect_cold_plasma(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    case WS2812_DEBUG_WARP_CHARGE:
        ws2812_effect_warp_charge(MODULE_WS2812_STRIP_0, debug_elapsed_ms);
        break;
    default:
        break;
    }
}
#endif

bool module_ws2812_debug_select(ws2812_debug_effect_e effect)
{
#ifdef DEBUG_ENABLE
    if ((uint32_t)effect > WS2812_DEBUG_WARP_CHARGE) {
        return false;
    }
    debug_ctrl.bits.wse = (uint8_t)effect;
    debug_elapsed_ms = 0U;
    return true;
#else
    (void)effect;
    return false;
#endif
}

/* 红色警戒灯：门动作时，前 8 颗 200ms 亮 / 200ms 灭 */
static void dev_door_actioning(uint16_t cyctime)
{
    for (int i = 0; i < 4; i++) {
        ws2812_flash_with(MODULE_WS2812_STRIP_0, (i << 1), (i << 1) + 1, WS2812_RGB(252, 3, 3), WS2812_RGB(0, 0, 0), -1, 90, &door_flash_state);
    }
    ws2812_flash_update(4, 4, -1, &door_flash_state);

}

/* 平台动作：尾部跑马灯 */
static void dev_platform_actioning(uint16_t cyctime)
{
    ws2812_run_with(MODULE_WS2812_STRIP_0, WS2812_RGB(255, 255, 255), 2, &platform_run_state);
}

/* 平台导航：四角白色定位 + 绿色边界 */
static void dev_platform_navigate(uint16_t cyctime)
{
    for (int i = 0; i < 4; i++) {
        ws2812_flash_with(MODULE_WS2812_STRIP_0, (i << 2), (i << 2) + 1,
            WS2812_RGB(255, 255, 255), WS2812_RGB(0, 0, 0), 2, 90, &door_flash_state);
    }
    ws2812_flash_update(2, 2, 2, &door_flash_state);
    module_ws2812_effect_at(MODULE_WS2812_STRIP_0, 8, 9, WS2812_RGB(0, 252, 93), 90);
}

static door_running_in_e door_running;
static platform_running_in_e platform_running;
static uint8_t ws2812_effect_active;

void set_door_running_in(door_running_in_e in)
{
    door_running = in;
}

void set_platform_running_in(platform_running_in_e in)
{
    platform_running = in;
}

void module_ws2812_status_update(uint16_t cyctime)
{
    bool any_effect = false;

    module_ws2812_clear_work_frame();

#ifdef DEBUG_ENABLE
    /* 调试灯效独立计时，WSE:0 时恢复业务状态。 */
    if (debug_ctrl.bits.wse != 0U) {
        ws2812_render_debug_effect((ws2812_debug_effect_e)debug_ctrl.bits.wse);
        debug_elapsed_ms += cyctime;
        ws2812_effect_active = 1U;
        module_ws2812_copy_work_frame_to_dma(MODULE_WS2812_STRIP_0);
        module_ws2812_send_frame_dma(MODULE_WS2812_STRIP_0);
        return;
    }
#endif

    switch (door_running) {
    case E_DOOR_RUN_IN_ACTION:
        dev_door_actioning(cyctime);
        any_effect = true;
        break;
    default:
        break;
    }

    switch (platform_running) {
    case E_PLATFORM_RUN_IN_ACTION:
        dev_platform_actioning(cyctime);
        any_effect = true;
        break;
    case E_PLATFORM_NAVIGATE:
        dev_platform_navigate(cyctime);
        any_effect = true;
        break;
    default:
        break;
    }

    ws2812_effect_active = any_effect ? 1U : 0U;
    module_ws2812_copy_work_frame_to_dma(MODULE_WS2812_STRIP_0);
    module_ws2812_send_frame_dma(MODULE_WS2812_STRIP_0);
}

uint8_t module_ws2812_effect_is_active(void)
{
    return ws2812_effect_active;
}
