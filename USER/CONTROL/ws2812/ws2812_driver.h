#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>

#include "BOARD/board.h"


#define MODULE_WS2812_STRIP0_LED_COUNT  12U
#define WS2812_ENCODED_BYTES_PER_LED  24U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODULE_WS2812_STRIP_0 = 0,

    MODULE_WS2812_ID_COUNT
} module_ws2812_id_e;

/* 颜色组合宏，格式 0xRRGGBB */
#define WS2812_RGB(r, g, b) \
    ((((uint32_t)(r) & 0xFFU) << 16U) | (((uint32_t)(g) & 0xFFU) << 8U)  | ((uint32_t)(b) & 0xFFU))

void module_ws2812_init(void);


typedef struct {
    board_spi_id_e spi;
    uint32_t led_count;
    uint8_t *dma_tx_buffer;
} module_ws2812_config_t;


/**
 * @brief 将一颗灯珠的 RGB 颜色编码到 encoded_frame
 *
 * 按本机灯带的 BRG 通道顺序和 MSB-first 位序，将每个颜色位编码为一个
 * SPI 字节。调用方需保证 encoded_frame 至少包含
 * (led_index + 1) * WS2812_ENCODED_BYTES_PER_LED 字节。
 */
void ws2812_frame_set_rgb(uint8_t *encoded_frame, uint32_t led_index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 设置指定灯带中一颗灯珠的 RGB 数据
 *
 * 颜色写入共享构帧缓冲区，调用方负责按灯带顺序完整构帧并提交发送。
 */
void module_ws2812_set_rgb(module_ws2812_id_e strip_id, uint32_t led_index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 将指定灯带中 [start_led, end_led] 区间的灯珠设置为同一颜色
 *
 * 颜色格式为 0xRRGGBB，可用 WS2812_RGB(r, g, b) 宏组合。
 * 颜色写入共享构帧缓冲区，调用方负责按灯带顺序完整构帧并提交发送。
 */
void module_ws2812_set_rgb_range(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color);

void module_ws2812_clear_work_frame(void);
void module_ws2812_copy_work_frame_to_dma(module_ws2812_id_e strip_id);

void module_ws2812_send_frame(module_ws2812_id_e strip_id);

/**
 * @brief 通过 SPI TX DMA 发送指定灯带的当前帧
 *
 * 函数启动 DMA 后立即返回。上一帧尚未产生 FTF 标志时，
 * 本次发送会被直接丢弃。
 */
void module_ws2812_send_frame_dma(module_ws2812_id_e strip_id);

/* DMA0_CH2 完成中断入口：等待 SPI 移位结束并释放 PA5。 */
void module_ws2812_dma_irq(void);


uint32_t get_strip_led_count(module_ws2812_id_e strip_id);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_DRIVER_H */
