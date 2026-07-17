#include "module_ws2812.h"



#include <stdbool.h>
#include <string.h>

#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"

#define WS2812_CODE_0                 0xC0U
#define WS2812_CODE_1                 0xF0U
#define WS2812_RESET_BYTES            237U
#define MODULE_WS2812_MAX_LED_COUNT   MODULE_WS2812_STRIP2_LED_COUNT
#define WS2812_WORK_FRAME_BYTES       (MODULE_WS2812_MAX_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_STRIP0_FRAME_BYTES     (MODULE_WS2812_STRIP0_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_STRIP1_FRAME_BYTES     (MODULE_WS2812_STRIP1_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_STRIP2_FRAME_BYTES     (MODULE_WS2812_STRIP2_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_DMA_TX_BUFFER0_BYTES   (WS2812_STRIP0_FRAME_BYTES + WS2812_RESET_BYTES)
#define WS2812_DMA_TX_BUFFER1_BYTES   (WS2812_STRIP1_FRAME_BYTES + WS2812_RESET_BYTES)
#define WS2812_DMA_TX_BUFFER2_BYTES   (WS2812_STRIP2_FRAME_BYTES + WS2812_RESET_BYTES)

static uint8_t ws2812_work_frame[WS2812_WORK_FRAME_BYTES];

static uint8_t ws2812_dma_tx_buffer0[WS2812_DMA_TX_BUFFER0_BYTES];
static uint8_t ws2812_dma_tx_buffer1[WS2812_DMA_TX_BUFFER1_BYTES];
static uint8_t ws2812_dma_tx_buffer2[WS2812_DMA_TX_BUFFER2_BYTES];



typedef struct {
    tf_drv_spi_handle_t spi_handle;
    tf_drv_dma_handle_t dma_handle;
    bool transfer_started;
} module_ws2812_device_t;

static const module_ws2812_config_t ws2812_configs[MODULE_WS2812_ID_COUNT] = {
    [MODULE_WS2812_STRIP_0] = {
        .spi_id = E_DRV_SPI_0,
        .dma_controller_id = E_DRV_DMA_0,
        .dma_channel_index = E_DRV_DX_CH2,
        .led_count = MODULE_WS2812_STRIP0_LED_COUNT,
        .dma_tx_buffer = ws2812_dma_tx_buffer0,
    },
    [MODULE_WS2812_STRIP_1] = {
        .spi_id = E_DRV_SPI_1,
        .dma_controller_id = E_DRV_DMA_0,
        .dma_channel_index = E_DRV_DX_CH4,
        .led_count = MODULE_WS2812_STRIP1_LED_COUNT,
        .dma_tx_buffer = ws2812_dma_tx_buffer1,
    },
    [MODULE_WS2812_STRIP_2] = {
        .spi_id = E_DRV_SPI_2,
        .dma_controller_id = E_DRV_DMA_1,
        .dma_channel_index = E_DRV_DX_CH1,
        .led_count = MODULE_WS2812_STRIP2_LED_COUNT,
        .dma_tx_buffer = ws2812_dma_tx_buffer2,
    },
};

static module_ws2812_device_t ws2812_devices[MODULE_WS2812_ID_COUNT];

static bool module_ws2812_id_is_valid(module_ws2812_id_e strip_id)
{
    return (uint32_t)strip_id < (uint32_t)MODULE_WS2812_ID_COUNT;
}

static uint32_t module_ws2812_prepare_dma_tx_buffer(
    const module_ws2812_config_t *ws2812_config)
{
    uint32_t frame_length;

    frame_length = ws2812_config->led_count * WS2812_ENCODED_BYTES_PER_LED;
    memcpy(ws2812_config->dma_tx_buffer, ws2812_work_frame, frame_length);

    return frame_length + WS2812_RESET_BYTES;
}

void ws2812_frame_set_rgb(uint8_t *encoded_frame, uint32_t led_index,
                          uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t *encoded_led;
    uint32_t grb_color;
    uint32_t bit_mask;

    if (encoded_frame == NULL) {
        return;
    }

    encoded_led = encoded_frame + led_index * WS2812_ENCODED_BYTES_PER_LED;
    grb_color = ((uint32_t)green << 16U) |
                ((uint32_t)red << 8U) |
                (uint32_t)blue;
    bit_mask = 1UL << 23U;

    for (uint32_t encoded_index = 0U;
         encoded_index < WS2812_ENCODED_BYTES_PER_LED;
         encoded_index++) {
        encoded_led[encoded_index] =
            (grb_color & bit_mask) != 0U ? WS2812_CODE_1 : WS2812_CODE_0;
        bit_mask >>= 1U;
    }
}

void module_ws2812_init(void)
{
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    (void)tf_drv_dma_register(E_DRV_DMA_0);
    (void)tf_drv_dma_register(E_DRV_DMA_1);

    for (int strip_index = 0; strip_index < MODULE_WS2812_ID_COUNT; strip_index++) {
        const module_ws2812_config_t *ws2812_config = &ws2812_configs[strip_index];
        module_ws2812_device_t *ws2812_device = &ws2812_devices[strip_index];

        ws2812_device->spi_handle = tf_drv_spi_register(ws2812_config->spi_id);
        ws2812_device->dma_handle = tf_drv_dma_get_handle(ws2812_config->dma_controller_id);
        ws2812_device->transfer_started = false;

        if (ws2812_device->spi_handle != NULL) {
            tf_hal_spi_dma_tx_config(spi_configs[ws2812_config->spi_id].base, 1U);
        }

        memset(ws2812_config->dma_tx_buffer, 0,
               ws2812_config->led_count * WS2812_ENCODED_BYTES_PER_LED +
               WS2812_RESET_BYTES);
    }
}

void module_ws2812_set_rgb(module_ws2812_id_e strip_id, uint32_t led_index,
                           uint8_t red, uint8_t green, uint8_t blue)
{
    const module_ws2812_config_t *ws2812_config;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    ws2812_config = &ws2812_configs[strip_id];
    if (led_index >= ws2812_config->led_count) {
        return;
    }

    ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
}

void module_ws2812_set_rgb_range(module_ws2812_id_e strip_id,
                                 uint32_t start_led,
                                 uint32_t end_led,
                                 uint32_t color)
{
    const module_ws2812_config_t *ws2812_config;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    ws2812_config = &ws2812_configs[strip_id];
    if ((start_led >= ws2812_config->led_count) || (end_led >= ws2812_config->led_count))
    {
        return;
    }

    red = (uint8_t)((color >> 16U) & 0xFFU);
    green = (uint8_t)((color >> 8U) & 0xFFU);
    blue = (uint8_t)(color & 0xFFU);

    if (start_led <= end_led)
    {
        for (uint32_t led_index = start_led; led_index <= end_led; led_index++)
        {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
    }
    else
    {
        for (uint32_t led_index = start_led; led_index < ws2812_config->led_count; led_index++)
        {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
        for (uint32_t led_index = 0U; led_index <= end_led; led_index++)
        {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
    }
}

void module_ws2812_clear_work_frame(void)
{
    memset(ws2812_work_frame, 0, sizeof(ws2812_work_frame));
}

void module_ws2812_copy_work_frame_to_dma(module_ws2812_id_e strip_id)
{
    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    (void)module_ws2812_prepare_dma_tx_buffer(&ws2812_configs[strip_id]);
}

void module_ws2812_send_frame(module_ws2812_id_e strip_id)
{
    const module_ws2812_config_t *ws2812_config;
    module_ws2812_device_t *ws2812_device;
    const tf_drv_dma_config_t *dma_config;
    uint8_t dma_hardware_channel;
    uint32_t dma_tx_buffer_length;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    ws2812_config = &ws2812_configs[strip_id];
    ws2812_device = &ws2812_devices[strip_id];
    if (ws2812_device->spi_handle == NULL) {
        return;
    }

    if (ws2812_device->transfer_started) {
        dma_config = &dma_configs[ws2812_config->dma_controller_id];
        dma_hardware_channel =
            (uint8_t)dma_config->ch[ws2812_config->dma_channel_index].channel;

        if (tf_hal_dma_flag_get(dma_config->base, dma_hardware_channel, DMA_FLAG_FTF) == 0U) {
            return;
        }

        ws2812_device->transfer_started = false;
    }

    dma_tx_buffer_length = module_ws2812_prepare_dma_tx_buffer(ws2812_config);
    tf_drv_spi_tx(ws2812_device->spi_handle, ws2812_config->dma_tx_buffer,
                  dma_tx_buffer_length);
}

void module_ws2812_send_frame_dma(module_ws2812_id_e strip_id)
{
    const module_ws2812_config_t *ws2812_config;
    module_ws2812_device_t *ws2812_device;
    const tf_drv_dma_config_t *dma_config;
    uint8_t dma_hardware_channel;
    uint32_t dma_tx_buffer_length;
    uint32_t spi_data_address;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    ws2812_config = &ws2812_configs[strip_id];
    ws2812_device = &ws2812_devices[strip_id];
    if (ws2812_device->spi_handle == NULL || ws2812_device->dma_handle == NULL) {
        return;
    }

    dma_config = &dma_configs[ws2812_config->dma_controller_id];
    dma_hardware_channel =
        (uint8_t)dma_config->ch[ws2812_config->dma_channel_index].channel;
    spi_data_address = dma_config->ch[ws2812_config->dma_channel_index].periph_addr;

    if (ws2812_device->transfer_started &&
        tf_hal_dma_flag_get(dma_config->base, dma_hardware_channel, DMA_FLAG_FTF) == 0U) {
        return;
    }

    dma_tx_buffer_length = module_ws2812_prepare_dma_tx_buffer(ws2812_config);
    tf_drv_dma_stop(ws2812_device->dma_handle, ws2812_config->dma_channel_index);
    tf_drv_dma_addr_config(ws2812_device->dma_handle, ws2812_config->dma_channel_index,
                           spi_data_address, (uint32_t)ws2812_config->dma_tx_buffer);
    tf_drv_dma_number_config(ws2812_device->dma_handle, ws2812_config->dma_channel_index,
                             dma_tx_buffer_length);
    tf_drv_dma_start(ws2812_device->dma_handle, ws2812_config->dma_channel_index);
    ws2812_device->transfer_started = true;
}




uint32_t get_strip_led_count(module_ws2812_id_e strip_id)
{
    return ws2812_configs[strip_id].led_count;
}


