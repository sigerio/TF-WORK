#include "APP/ws2812_app.h"

#include <stdbool.h>

#include "DEBUG/debug_log.h"
#include "APP/app_fault.h"
#include "COMM/modbus/modbus_master.h"
#include "CONTROL/ws2812/effects/ws2812_basic.h"
#include "CONTROL/ws2812/ws2812_frame.h"
#include "CONTROL/ws2812/driver/ws2812_driver.h"

/* SPI0/PA7 的 WS2812 灯效按机库业务状态驱动。 */

/* 数值越大显示优先级越高，仅在灯光应用内部使用。 */
typedef enum {
    WS2812_ALARM_NONE = 0,
    WS2812_ALARM_WATER,
    WS2812_ALARM_LIMIT,
    WS2812_ALARM_ESTOP
} ws2812_alarm_e;

typedef struct {
    door_running_in_e door_running;
    platform_running_in_e platform_running;
    ws2812_flash_state_t door_flash;
    ws2812_trail_state_t platform_trail;
    uint32_t debug_elapsed_ms;
    ws2812_alarm_e alarm;
    ws2812_alarm_e pending_alarm;
    uint32_t alarm_elapsed_ms;
    uint32_t alarm_recovery_ms;
    bool water_alarm_held;
    uint8_t effect_active;
    uint32_t pixels[WS2812_STRIP0_LED_COUNT];
    ws2812_frame_t frame;
} ws2812_app_state_t;

static ws2812_app_state_t ws2812_app = {
    .frame = { .pixels = ws2812_app.pixels, .led_count = WS2812_STRIP0_LED_COUNT },
};

static bool ws2812_render_alarm(uint16_t cycle_ms)
{
    enum { recovery_ms = 2000U, phase_wrap_ms = 3000U }; /* 双闪 1500 和交替 1000 ms 周期的公倍数。 */
    app_fault_u faults = app_fault_get();
    ws2812_alarm_e requested = WS2812_ALARM_NONE;
    const uint32_t alarm_color = WS2812_RGB(255U, 0U, 0U);

    /* 已确认水浸后，失联或非法数据不能证明无水；只在有效无水时释放显示保持。 */
    if (faults.bits.water_detected) ws2812_app.water_alarm_held = true;
    else if (!faults.bits.water_offline && !faults.bits.water_sensor_fault) ws2812_app.water_alarm_held = false;

    if (faults.bits.emergency_stop) requested = WS2812_ALARM_ESTOP;
    else if (faults.bits.door_limit_conflict || faults.bits.platform_limit_conflict) requested = WS2812_ALARM_LIMIT;
    else if (ws2812_app.water_alarm_held) requested = WS2812_ALARM_WATER;

    if (requested >= ws2812_app.alarm) {
        ws2812_app.alarm_recovery_ms = 0U;
        ws2812_app.pending_alarm = requested;
        if (requested != ws2812_app.alarm) {
            ws2812_app.alarm = requested;
            ws2812_app.alarm_elapsed_ms = 0U;
        }
    } else {
        if (requested != ws2812_app.pending_alarm) {
            ws2812_app.pending_alarm = requested;
            ws2812_app.alarm_recovery_ms = 0U;
        } else {
            ws2812_app.alarm_recovery_ms += cycle_ms;
        }
        if (ws2812_app.alarm_recovery_ms >= recovery_ms) {
            ws2812_app.alarm = requested;
            ws2812_app.alarm_elapsed_ms = 0U;
            ws2812_app.alarm_recovery_ms = 0U;
        }
    }

    switch (ws2812_app.alarm) {
    case WS2812_ALARM_ESTOP:
        ws2812_frame_fill(&ws2812_app.frame, 0U, ws2812_app.frame.led_count - 1U, alarm_color);
        break;
    case WS2812_ALARM_LIMIT:
        ws2812_effect_double_flash(&ws2812_app.frame, alarm_color, ws2812_app.alarm_elapsed_ms);
        break;
    case WS2812_ALARM_WATER:
        ws2812_effect_alternating_halves(&ws2812_app.frame, alarm_color, ws2812_app.alarm_elapsed_ms);
        break;
    default:
        return false;
    }
    ws2812_app.alarm_elapsed_ms = (ws2812_app.alarm_elapsed_ms + cycle_ms) % phase_wrap_ms;
    return true;
}

#ifdef DEBUG_ENABLE
static void ws2812_render_debug_effect(ws2812_debug_effect_e effect)
{
    switch (effect) {
    case WS2812_DEBUG_ESTOP:
        ws2812_frame_fill(&ws2812_app.frame, 0U, ws2812_app.frame.led_count - 1U, WS2812_RGB(255U, 0U, 0U));
        break;
    case WS2812_DEBUG_LIMIT_CONFLICT:
        ws2812_effect_double_flash(&ws2812_app.frame, WS2812_RGB(255U, 0U, 0U), ws2812_app.debug_elapsed_ms);
        break;
    case WS2812_DEBUG_WATER:
        ws2812_effect_alternating_halves(&ws2812_app.frame, WS2812_RGB(255U, 0U, 0U), ws2812_app.debug_elapsed_ms);
        break;
    default:
        break;
    }
}
#endif

bool ws2812_app_select_debug(ws2812_debug_effect_e effect)
{
#ifdef DEBUG_ENABLE
    if ((uint32_t)effect > WS2812_DEBUG_WATER) {
        return false;
    }
    app_fault_u simulated = {0};
    switch (effect) {
    case WS2812_DEBUG_ESTOP: simulated.bits.emergency_stop = 1U; break;
    case WS2812_DEBUG_LIMIT_CONFLICT: simulated.bits.door_limit_conflict = 1U; break;
    case WS2812_DEBUG_WATER: simulated.bits.water_detected = 1U; break;
    default: break;
    }
    debug_ctrl.bits.wse = (uint8_t)effect;
    ws2812_app.debug_elapsed_ms = 0U;
    modbus_master_set_weather_fault_debug(simulated.raw);
    return true;
#else
    (void)effect;
    return false;
#endif
}

/* 红色警戒灯：门动作时，前 8 颗 200ms 亮 / 200ms 灭 */
static void ws2812_render_door_motion(void)
{
    for (int i = 0; i < 4; i++) {
        ws2812_effect_flash(&ws2812_app.frame, (i << 1), (i << 1) + 1, WS2812_RGB(252, 3, 3), WS2812_RGB(0, 0, 0), -1, 90, &ws2812_app.door_flash);
    }
    ws2812_flash_update(4, 4, -1, &ws2812_app.door_flash);

}

/* 平台动作：尾部跑马灯 */
static void ws2812_render_platform_motion(void)
{
    ws2812_effect_trail(&ws2812_app.frame, WS2812_RGB(255, 255, 255), 2, &ws2812_app.platform_trail);
}

/* 平台导航：四角白色定位 + 绿色边界 */
static void ws2812_render_platform_navigation(void)
{
    for (int i = 0; i < 4; i++) {
        ws2812_effect_flash(&ws2812_app.frame, (i << 2), (i << 2) + 1,
            WS2812_RGB(255, 255, 255), WS2812_RGB(0, 0, 0), 2, 90, &ws2812_app.door_flash);
    }
    ws2812_flash_update(2, 2, 2, &ws2812_app.door_flash);
    ws2812_effect_solid(&ws2812_app.frame, 8, 9, WS2812_RGB(0, 252, 93), 90);
}


void ws2812_app_set_door_state(door_running_in_e in)
{
    ws2812_app.door_running = in;
}

void ws2812_app_set_platform_state(platform_running_in_e in)
{
    ws2812_app.platform_running = in;
}

void ws2812_app_update(uint16_t cyctime)
{
    bool any_effect = false;

    ws2812_frame_clear(&ws2812_app.frame);

    if (ws2812_render_alarm(cyctime)) {
        ws2812_app.effect_active = 1U;
        (void)ws2812_driver_submit(WS2812_STRIP_0, &ws2812_app.frame);
        return;
    }

#ifdef DEBUG_ENABLE
    /* 调试灯效独立计时，WSE:0 时恢复业务状态。 */
    if (debug_ctrl.bits.wse != 0U) {
        ws2812_render_debug_effect((ws2812_debug_effect_e)debug_ctrl.bits.wse);
        ws2812_app.debug_elapsed_ms += cyctime;
        ws2812_app.effect_active = 1U;
        (void)ws2812_driver_submit(WS2812_STRIP_0, &ws2812_app.frame);
        return;
    }
#endif

    switch (ws2812_app.door_running) {
    case E_DOOR_RUN_IN_ACTION:
        ws2812_render_door_motion();
        any_effect = true;
        break;
    default:
        break;
    }

    switch (ws2812_app.platform_running) {
    case E_PLATFORM_RUN_IN_ACTION:
        ws2812_render_platform_motion();
        any_effect = true;
        break;
    case E_PLATFORM_NAVIGATE:
        ws2812_render_platform_navigation();
        any_effect = true;
        break;
    default:
        break;
    }

    ws2812_app.effect_active = any_effect ? 1U : 0U;
    (void)ws2812_driver_submit(WS2812_STRIP_0, &ws2812_app.frame);
}

uint8_t ws2812_app_is_active(void)
{
    return ws2812_app.effect_active;
}
