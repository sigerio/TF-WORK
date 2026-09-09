#ifndef WS2812_STATUS_H
#define WS2812_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODULE_WS2812_PERIOD_MS 50U /* 灯效状态机调度周期（ms）。 */

/* WSE 调试命令编号；0 恢复门/平台业务灯效。 */
typedef enum {
    WS2812_DEBUG_AUTO = 0,
    WS2812_DEBUG_REACTOR_HEARTBEAT,
    WS2812_DEBUG_DUAL_ROTOR,
    WS2812_DEBUG_HEAVY_SCAN,
    WS2812_DEBUG_SHOCKWAVE,
    WS2812_DEBUG_COLD_PLASMA,
    WS2812_DEBUG_WARP_CHARGE,
} ws2812_debug_effect_e;

/* 选择调试灯效并从头播放；编号无效或未启用调试时返回 false。 */
bool module_ws2812_debug_select(ws2812_debug_effect_e effect);

/* 门业务状态对灯效层的简化输入，不表示门的开/关终态。 */
typedef enum {
    E_DOOR_RUN_IDLE = 0,
    E_DOOR_RUN_IN_ACTION,
} door_running_in_e;

/* 平台业务状态对灯效层的简化输入。 */
typedef enum {
    E_PLATFORM_RUN_IDLE = 0,
    E_PLATFORM_RUN_IN_ACTION,
    E_PLATFORM_NAVIGATE,
} platform_running_in_e;

/* 更新门/平台状态输入；由用户事件任务在业务状态变化时调用。 */
void set_door_running_in(door_running_in_e in);
void set_platform_running_in(platform_running_in_e in);

/* 推进 WS2812 灯效；cycle_ms 必须等于实际调用周期。 */
void module_ws2812_status_update(uint16_t);

/* 返回当前是否正在输出业务或调试灯效（包括常亮）。 */
uint8_t module_ws2812_effect_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_STATUS_H */
