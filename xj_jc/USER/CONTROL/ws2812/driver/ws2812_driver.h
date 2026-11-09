#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdbool.h>
#include "CONTROL/ws2812/ws2812_types.h"

/* 物理灯珠数：同时决定应用 RGB 帧和驱动 DMA 缓冲大小，修改后重新编译。 */
#define WS2812_STRIP0_LED_COUNT 12U
#define WS2812_ENCODED_BYTES_PER_LED 24U

typedef enum {
    WS2812_STRIP_0 = 0,
    WS2812_STRIP_COUNT,
} ws2812_strip_e;

#ifdef __cplusplus
extern "C" {
#endif

void ws2812_driver_init(void);
/* 检查 DMA 空闲后编码并启动发送，忙/参数无效返回 false，不改写发送缓冲区。
 * frame 必须包含目标灯带全部灯珠；返回后调用者可复用 RGB 帧，不排队或重试。 */
bool ws2812_driver_submit(ws2812_strip_e strip_id, const ws2812_frame_t *frame);
/* DMA0_CH2 完成后等待 SPI 移位结束并释放 PA5。 */
void ws2812_driver_dma_irq(void);
/* 单灯 RGB -> BRG/MSB-first SPI 符号。调用者保证缓冲至少容纳 (index + 1) * 24 字节。 */
void ws2812_driver_encode_pixel(uint8_t *encoded_frame, uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif
#endif /* WS2812_DRIVER_H */
