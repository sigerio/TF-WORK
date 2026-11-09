#include "CONTROL/ws2812/driver/ws2812_driver.h"
#include "BOARD/board.h"

#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_spi.h"

#include <stdbool.h>
#include <string.h>


#define WS2812_CODE_0 0xC0U
#define WS2812_CODE_1 0xF0U
#define WS2812_RESET_BYTES 237U
#define WS2812_STRIP0_FRAME_BYTES (WS2812_STRIP0_LED_COUNT * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_DMA_TX_BUFFER0_BYTES (WS2812_STRIP0_FRAME_BYTES + WS2812_RESET_BYTES)

typedef struct {
    volatile bool transfer_started;
    uint8_t dma_tx_buffer[WS2812_DMA_TX_BUFFER0_BYTES];
} ws2812_driver_state_t;

typedef struct {
    board_spi_id_e spi;
    uint32_t led_count;
} ws2812_driver_config_t;

static const ws2812_driver_config_t ws2812_configs[WS2812_STRIP_COUNT] = {
    [WS2812_STRIP_0] = { .spi = BOARD_SPI_WS2812, .led_count = WS2812_STRIP0_LED_COUNT },
};
static ws2812_driver_state_t ws2812_devices[WS2812_STRIP_COUNT];

static void ws2812_sck_spi_mode(const board_spi_config_t *spi)
{
    const board_gpio_config_t *sck = &board_gpio_configs[spi->sck_pin];

    gpio_init(sck->port, GPIO_MODE_AF_PP, sck->speed, sck->pin);
}

static void ws2812_sck_platform_input_mode(const board_spi_config_t *spi)
{
    const board_gpio_config_t *sck = &board_gpio_configs[spi->sck_pin];

    gpio_init(sck->port, GPIO_MODE_IPU, sck->speed, sck->pin);
}

static void ws2812_hardware_init(void)
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

    for (int strip_index = 0; strip_index < WS2812_STRIP_COUNT; strip_index++) {
        const ws2812_driver_config_t *config = &ws2812_configs[strip_index];
        const board_spi_config_t *spi = &board_spi_configs[config->spi];
        const board_gpio_config_t *mosi_pin = &board_gpio_configs[spi->mosi_pin];
        const board_dma_config_t *dma_ctl = &board_dma_configs[spi->tx_dma.dma];
        const board_dma_channel_config_t *dma = &dma_ctl->ch[spi->tx_dma.ch];
        spi_parameter_struct spi_parameters;

        /* WS2812 不接外部 SCK，但 SPI 内部移位仍要求 SCK 复用有效。 */
        ws2812_sck_spi_mode(spi);
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
        ws2812_sck_platform_input_mode(spi);
    }
}

void ws2812_driver_encode_pixel(uint8_t *encoded_frame, uint32_t led_index, uint8_t red, uint8_t green, uint8_t blue)
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

void ws2812_driver_init(void)
{
    ws2812_hardware_init();
    memset(ws2812_devices, 0, sizeof(ws2812_devices));
}

bool ws2812_driver_submit(ws2812_strip_e strip_id, const ws2812_frame_t *frame)
{
    const ws2812_driver_config_t *config;
    ws2812_driver_state_t *device;
    const board_spi_config_t *spi;
    const board_dma_config_t *dma_ctl;
    const board_dma_channel_config_t *dma;
    uint32_t dma_tx_buffer_length;

    if ((uint32_t)strip_id >= WS2812_STRIP_COUNT || frame == NULL || frame->pixels == NULL) {
        return false;
    }

    config = &ws2812_configs[strip_id];
    if (frame->led_count != config->led_count) {
        return false;
    }
    device = &ws2812_devices[strip_id];
    spi = &board_spi_configs[config->spi];
    dma_ctl = &board_dma_configs[spi->tx_dma.dma];
    dma = &dma_ctl->ch[spi->tx_dma.ch];

    if (device->transfer_started && (dma_flag_get(dma_ctl->controller, dma->channel, DMA_FLAG_FTF) == RESET)) {
        return false;
    }
    /* 完成标志可能早于中断处理；先收尾旧传输，避免旧中断关闭新一帧。 */
    ws2812_driver_dma_irq();

    /* 只有 DMA 已空闲才能覆盖发送缓冲；调用返回后应用可立即修改 RGB 帧。 */
    for (uint32_t index = 0U; index < frame->led_count; index++) {
        uint32_t color = frame->pixels[index];
        ws2812_driver_encode_pixel(device->dma_tx_buffer, index, (uint8_t)(color >> 16U), (uint8_t)(color >> 8U), (uint8_t)color);
    }
    dma_tx_buffer_length = frame->led_count * WS2812_ENCODED_BYTES_PER_LED + WS2812_RESET_BYTES;
    ws2812_sck_spi_mode(spi);
    dma_channel_disable(dma_ctl->controller, dma->channel);
    dma_periph_address_config(dma_ctl->controller, dma->channel, dma->peripheral_address);
    dma_memory_address_config(dma_ctl->controller, dma->channel, (uint32_t)device->dma_tx_buffer);
    dma_transfer_number_config(dma_ctl->controller, dma->channel, dma_tx_buffer_length);
    /* FTF/HTF/ERR 不会因重新装载计数器自动清除，启动前必须全部清掉。 */
    dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    /* 防止其他代码关闭 SPI DMA 请求，发送前显式恢复 TX DMA。 */
    spi_dma_enable(spi->peripheral, SPI_DMA_TRANSMIT);
    device->transfer_started = true;
    dma_channel_enable(dma_ctl->controller, dma->channel);
    return true;
}

void ws2812_driver_dma_irq(void)
{
    const ws2812_driver_config_t *config = &ws2812_configs[WS2812_STRIP_0];
    ws2812_driver_state_t *device = &ws2812_devices[WS2812_STRIP_0];
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
    ws2812_sck_platform_input_mode(spi);
    device->transfer_started = false;
}
