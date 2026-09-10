#ifndef WS2812_MECHANICAL_H
#define WS2812_MECHANICAL_H

#include "CONTROL/ws2812/ws2812_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 只绘制 RGB 帧；elapsed_ms 为播放时间，传入 0 从头开始，不持有全局状态。
 * 位置随 frame->led_count 缩放；每个实现函数开头集中列出时间、空间宽度、亮度和颜色参数。
 * _ms 为毫秒，_pos 为归一化位置单位，_level 为 0..255，_percent 为百分比。
 * 改变灯珠数保持整条灯带的周期和相对宽度，不保持每秒经过的灯珠数。 */
void ws2812_effect_reactor_heartbeat(ws2812_frame_t *frame, uint32_t elapsed_ms);
void ws2812_effect_dual_rotor(ws2812_frame_t *frame, uint32_t elapsed_ms);
void ws2812_effect_heavy_scan(ws2812_frame_t *frame, uint32_t elapsed_ms);
void ws2812_effect_shockwave(ws2812_frame_t *frame, uint32_t elapsed_ms);
void ws2812_effect_cold_plasma(ws2812_frame_t *frame, uint32_t elapsed_ms);
void ws2812_effect_warp_charge(ws2812_frame_t *frame, uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif
#endif /* WS2812_MECHANICAL_H */
