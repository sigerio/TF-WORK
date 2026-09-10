#ifndef WS2812_FRAME_H
#define WS2812_FRAME_H

#include "CONTROL/ws2812/ws2812_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ws2812_frame_clear(ws2812_frame_t *frame);
/* 闭区间填色；first > last 时跨越灯带尾部。无效帧或越界时不修改。 */
void ws2812_frame_fill(ws2812_frame_t *frame, uint32_t first, uint32_t last, uint32_t color);

#ifdef __cplusplus
}
#endif
#endif /* WS2812_FRAME_H */
