#ifndef MODULE_WS2812_H
#define MODULE_WS2812_H

#include <stdint.h>

#include "tf_drv_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

void ws2812_init_all(void);
void ws2812_send_frame(tf_drv_spi_handle_t h_spi,
                       const uint8_t *frame, uint32_t led_count);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_WS2812_H */
