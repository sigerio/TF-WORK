#include "CONTROL/ws2812/ws2812_driver.h"

#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_spi.h"

#include <stdbool.h>
#include <string.h>


#define WS2812_CODE_0 0xC0U
#define WS2812_CODE_1 0xF0U
#define WS2812_RESET_BYTES 237U
#define WS2812_STRIP0_FRAME_BYTES (MODULE_WS2812_STRIP0_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_WORK_FRAME_BYTES WS2812_STRIP0_FRAME_BYTES
#define WS2812_DMA_TX_BUFFER0_BYTES (WS2812_STRIP0_FRAME_BYTES + WS2812_RESET_BYTES)

static uint8_t ws2812_work_frame[WS2812_WORK_FRAME_BYTES];
static uint8_t ws2812_dma_tx_buffer0[WS2812_DMA_TX_BUFFER0_BYTES];

typedef struct {
    bool transfer_started;
} module_ws2812_device_t;

static const module_ws2812_config_t ws2812_configs[MODULE_WS2812_ID_COUNT] = {
    [MODULE_WS2812_STRIP_0] = {
        .spi = BOARD_SPI_WS2812,
        .led_count = MODULE_WS2812_STRIP0_LED_COUNT,
        .dma_tx_buffer = ws2812_dma_tx_buffer0,
    },
};

static module_ws2812_device_t ws2812_devices[MODULE_WS2812_ID_COUNT];

static void module_ws2812_sck_spi_mode(const board_spi_config_t *spi)
{
    const board_gpio_config_t *sck = &board_gpio_configs[spi->sck_pin];

    gpio_init(sck->port, GPIO_MODE_AF_PP, sck->speed, sck->pin);
}

static void module_ws2812_sck_platform_input_mode(const board_spi_config_t *spi)
{
    const board_gpio_config_t *sck = &board_gpio_configs[spi->sck_pin];

    gpio_init(sck->port, GPIO_MODE_IPU, sck->speed, sck->pin);
}

static void module_ws2812_hardware_init(void)
{
    dma_parameter_struct dma_parameters;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_AF);
    /* PA5/PA7 使用 SPI0 默认映射；PB3 仍由 UART4 485 方向控制使用。 */
    gpio_pin_remap_config(GPIO_SPI0_REMAP, DISABLE);

    dma_struct_para_init(&dma_parameters);
    dma_parameters.memory_addr = 0U;
    dma_parameters.number = 0U;
    dma_parameters.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_parameters.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_parameters.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_parameters.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_parameters.priority = DMA_PRIORITY_ULTRA_HIGH;

    for (int strip_index = 0; strip_index < MODULE_WS2812_ID_COUNT; strip_index++) {
        const module_ws2812_config_t *config = &ws2812_configs[strip_index];
        const board_spi_config_t *spi = &board_spi_configs[config->spi];
        const board_gpio_config_t *mosi_pin = &board_gpio_configs[spi->mosi_pin];
        const board_dma_config_t *dma_ctl = &board_dma_configs[spi->tx_dma.dma];
        const board_dma_channel_config_t *dma = &dma_ctl->ch[spi->tx_dma.ch];
        spi_parameter_struct spi_parameters;

        /* WS2812 不接外部 SCK，但 SPI 内部移位仍要求 SCK 复用有效。 */
        module_ws2812_sck_spi_mode(spi);
        gpio_init(mosi_pin->port, mosi_pin->mode, mosi_pin->speed, mosi_pin->pin);
        gpio_bit_write(mosi_pin->port, mosi_pin->pin, mosi_pin->initial_level ? SET : RESET);

        rcu_periph_clock_enable(spi->clock);
        spi_struct_para_init(&spi_parameters);
        spi_parameters.device_mode = SPI_MASTER;
        spi_parameters.trans_mode = SPI_TRANSMODE_BDTRANSMIT;
        spi_parameters.frame_size = SPI_FRAMESIZE_8BIT;
        spi_parameters.nss = SPI_NSS_SOFT;
        spi_parameters.endian = SPI_ENDIAN_MSB;
        spi_parameters.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
        spi_parameters.prescale = spi->prescale;
        spi_init(spi->peripheral, &spi_parameters);
        spi_dma_enable(spi->peripheral, SPI_DMA_TRANSMIT);
        spi_enable(spi->peripheral);

        dma_parameters.periph_addr = dma->peripheral_address;
        dma_parameters.direction = dma->direction;
        dma_deinit(dma_ctl->controller, dma->channel);
        dma_init(dma_ctl->controller, dma->channel, &dma_parameters);
        dma_circulation_disable(dma_ctl->controller, dma->channel);
        dma_memory_to_memory_disable(dma_ctl->controller, dma->channel);
        dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
        dma_interrupt_enable(dma_ctl->controller, dma->channel, DMA_INT_FTF);
        nvic_irq_enable(DMA0_Channel2_IRQn, 1U, 3U);
        module_ws2812_sck_platform_input_mode(spi);
    }
}

static bool module_ws2812_id_is_valid(module_ws2812_id_e strip_id)
{
    return (uint32_t)strip_id < (uint32_t)MODULE_WS2812_ID_COUNT;
}

static uint32_t module_ws2812_prepare_dma_tx_buffer(const module_ws2812_config_t *ws2812_config)
{
    uint32_t frame_length = ws2812_config->led_count * WS2812_ENCODED_BYTES_PER_LED;

    memcpy(ws2812_config->dma_tx_buffer, ws2812_work_frame, frame_length);
    return frame_length + WS2812_RESET_BYTES;
}

static void module_ws2812_spi_send(uint32_t spi, const uint8_t *data, uint32_t length)
{
    for (uint32_t index = 0U; index < length; index++) {
        while (spi_i2s_flag_get(spi, SPI_FLAG_TBE) == RESET) {
        }
        spi_i2s_data_transmit(spi, data[index]);
    }

    while (spi_i2s_flag_get(spi, SPI_FLAG_TRANS) == SET) {
    }
}

void ws2812_frame_set_rgb(uint8_t *encoded_frame, uint32_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t *encoded_led;
    uint32_t brg_color;
    uint32_t bit_mask;

    if (encoded_frame == NULL) {
        return;
    }

    encoded_led = encoded_frame + led_index * WS2812_ENCODED_BYTES_PER_LED;
    brg_color = ((uint32_t)blue << 16U) | ((uint32_t)red << 8U) | (uint32_t)green;
    bit_mask = 1UL << 23U;

    for (uint32_t encoded_index = 0U; encoded_index < WS2812_ENCODED_BYTES_PER_LED; encoded_index++) {
        encoded_led[encoded_index] = (brg_color & bit_mask) != 0U ? WS2812_CODE_1 : WS2812_CODE_0;
        bit_mask >>= 1U;
    }
}

void module_ws2812_init(void)
{
    module_ws2812_hardware_init();

    /* 工作帧已经是 SPI 编码帧，逻辑 0 必须编码为 0xC0，不能填 0x00。 */
    memset(ws2812_work_frame, WS2812_CODE_0, sizeof(ws2812_work_frame));

    for (int strip_index = 0; strip_index < MODULE_WS2812_ID_COUNT; strip_index++) {
        const module_ws2812_config_t *config = &ws2812_configs[strip_index];

        ws2812_devices[strip_index].transfer_started = false;
        memset(config->dma_tx_buffer, 0, config->led_count * WS2812_ENCODED_BYTES_PER_LED + WS2812_RESET_BYTES);
    }
}

void module_ws2812_set_rgb(module_ws2812_id_e strip_id, uint32_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    const module_ws2812_config_t *config;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    config = &ws2812_configs[strip_id];
    if (led_index >= config->led_count) {
        return;
    }

    ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
}

void module_ws2812_set_rgb_range(module_ws2812_id_e strip_id, uint32_t start_led, uint32_t end_led, uint32_t color)
{
    const module_ws2812_config_t *config;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    config = &ws2812_configs[strip_id];
    if ((start_led >= config->led_count) || (end_led >= config->led_count)) {
        return;
    }

    red = (uint8_t)((color >> 16U) & 0xFFU);
    green = (uint8_t)((color >> 8U) & 0xFFU);
    blue = (uint8_t)(color & 0xFFU);

    if (start_led <= end_led) {
        for (uint32_t led_index = start_led; led_index <= end_led; led_index++) {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
    } else {
        for (uint32_t led_index = start_led; led_index < config->led_count; led_index++) {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
        for (uint32_t led_index = 0U; led_index <= end_led; led_index++) {
            ws2812_frame_set_rgb(ws2812_work_frame, led_index, red, green, blue);
        }
    }
}

void module_ws2812_clear_work_frame(void)
{
    /* 编码后的逻辑 0 为 0xC0，填 0x00 会让 MOSI 长时间保持低电平。 */
    memset(ws2812_work_frame, WS2812_CODE_0, sizeof(ws2812_work_frame));
}

void module_ws2812_copy_work_frame_to_dma(module_ws2812_id_e strip_id)
{
    if (module_ws2812_id_is_valid(strip_id)) {
        (void)module_ws2812_prepare_dma_tx_buffer(&ws2812_configs[strip_id]);
    }
}

void module_ws2812_send_frame(module_ws2812_id_e strip_id)
{
    const module_ws2812_config_t *config;
    module_ws2812_device_t *device;
    const board_spi_config_t *spi;
    const board_dma_config_t *dma_ctl;
    const board_dma_channel_config_t *dma;
    uint32_t dma_tx_buffer_length;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    config = &ws2812_configs[strip_id];
    device = &ws2812_devices[strip_id];
    spi = &board_spi_configs[config->spi];
    dma_ctl = &board_dma_configs[spi->tx_dma.dma];
    dma = &dma_ctl->ch[spi->tx_dma.ch];

    if (device->transfer_started) {
        if (dma_flag_get(dma_ctl->controller, dma->channel, DMA_FLAG_FTF) == RESET) {
            return;
        }
        device->transfer_started = false;
    }

    dma_tx_buffer_length = module_ws2812_prepare_dma_tx_buffer(config);
    module_ws2812_spi_send(board_spi_configs[config->spi].peripheral, config->dma_tx_buffer, dma_tx_buffer_length);
}

void module_ws2812_send_frame_dma(module_ws2812_id_e strip_id)
{
    const module_ws2812_config_t *config;
    module_ws2812_device_t *device;
    const board_spi_config_t *spi;
    const board_dma_config_t *dma_ctl;
    const board_dma_channel_config_t *dma;
    uint32_t dma_tx_buffer_length;

    if (!module_ws2812_id_is_valid(strip_id)) {
        return;
    }

    config = &ws2812_configs[strip_id];
    device = &ws2812_devices[strip_id];
    spi = &board_spi_configs[config->spi];
    dma_ctl = &board_dma_configs[spi->tx_dma.dma];
    dma = &dma_ctl->ch[spi->tx_dma.ch];

    if (device->transfer_started && (dma_flag_get(dma_ctl->controller, dma->channel, DMA_FLAG_FTF) == RESET)) {
        return;
    }

    dma_tx_buffer_length = module_ws2812_prepare_dma_tx_buffer(config);
    module_ws2812_sck_spi_mode(spi);
    dma_channel_disable(dma_ctl->controller, dma->channel);
    dma_periph_address_config(dma_ctl->controller, dma->channel, dma->peripheral_address);
    dma_memory_address_config(dma_ctl->controller, dma->channel, (uint32_t)config->dma_tx_buffer);
    dma_transfer_number_config(dma_ctl->controller, dma->channel, dma_tx_buffer_length);
    /* FTF/HTF/ERR 不会因重新装载计数器自动清除，启动前必须全部清掉。 */
    dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    /* 防止其他代码关闭 SPI DMA 请求，发送前显式恢复 TX DMA。 */
    spi_dma_enable(spi->peripheral, SPI_DMA_TRANSMIT);
    dma_channel_enable(dma_ctl->controller, dma->channel);
    device->transfer_started = true;
}

void module_ws2812_dma_irq(void)
{
    const module_ws2812_config_t *config = &ws2812_configs[MODULE_WS2812_STRIP_0];
    module_ws2812_device_t *device = &ws2812_devices[MODULE_WS2812_STRIP_0];
    const board_spi_config_t *spi = &board_spi_configs[config->spi];
    const board_dma_config_t *dma_ctl = &board_dma_configs[spi->tx_dma.dma];
    const board_dma_channel_config_t *dma = &dma_ctl->ch[spi->tx_dma.ch];

    if (!device->transfer_started || dma_flag_get(dma_ctl->controller, dma->channel, DMA_FLAG_FTF) == RESET) {
        return;
    }

    while (spi_i2s_flag_get(spi->peripheral, SPI_FLAG_TRANS) == SET) {
    }
    spi_dma_disable(spi->peripheral, SPI_DMA_TRANSMIT);
    dma_channel_disable(dma_ctl->controller, dma->channel);
    dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    module_ws2812_sck_platform_input_mode(spi);
    device->transfer_started = false;
}

uint32_t get_strip_led_count(module_ws2812_id_e strip_id)
{
    return ws2812_configs[strip_id].led_count;
}
