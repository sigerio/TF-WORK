#ifndef WS2812_APP_H
#define WS2812_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS2812_APP_PERIOD_MS 50U /* 灯效状态机调度周期（ms）。 */

/* WSE 调试命令编号；0 恢复门/平台业务灯效。 */
typedef enum {
    WS2812_DEBUG_AUTO = 0,
    WS2812_DEBUG_ESTOP,          /* 1：急停，红色常亮。 */
    WS2812_DEBUG_LIMIT_CONFLICT, /* 2：门/平台限位冲突，红色双闪。 */
    WS2812_DEBUG_WATER,          /* 3：水浸，两半红色交替。 */
} ws2812_debug_effect_e;

/* 选择调试灯效并同步 B4 模拟故障位；0 恢复真实报文，P1 灯语期间仅保存灯效选择。
   不修改本机故障位；编号无效或未启用调试时返回 false。 */
bool ws2812_app_select_debug(ws2812_debug_effect_e effect);

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
void ws2812_app_set_door_state(door_running_in_e in);
void ws2812_app_set_platform_state(platform_running_in_e in);

/* 推进 WS2812 灯效：P1 灯语 > 调试 > 业务；cycle_ms 必须等于实际调用周期。 */
void ws2812_app_update(uint16_t cycle_ms);

/* 返回当前是否正在输出故障、业务或调试灯效（包括常亮与闪烁灭灯阶段）。 */
uint8_t ws2812_app_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_APP_H */
