/* Real driver with mocked registers: busy submission must not alter DMA-owned bytes. */
#include <assert.h>
#include <string.h>
#include "CONTROL/ws2812/driver/ws2812_driver.c"

const board_spi_config_t board_spi_configs[BOARD_SPI_COUNT] = {
    [BOARD_SPI_WS2812] = { .sck_pin = BOARD_PIN_PLATFORM_RETRACT_SIGNAL, .tx_dma = { BOARD_DMA_0, BOARD_DMA_CH2 } },
};
const board_gpio_config_t board_gpio_configs[BOARD_PIN_COUNT] = {0};
const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT] = {0};
static FlagStatus completed;
static unsigned starts;
static unsigned transfer_size;
static uint32_t pin_mode;

FlagStatus dma_flag_get(uint32_t dma, dma_channel_enum channel, uint32_t flag) { return completed; }
void dma_flag_clear(uint32_t dma, dma_channel_enum channel, uint32_t flags) { completed = RESET; }
void dma_channel_disable(uint32_t dma, dma_channel_enum channel) {}
void dma_channel_enable(uint32_t dma, dma_channel_enum channel) { starts++; }
void dma_periph_address_config(uint32_t dma, dma_channel_enum channel, uint32_t address) {}
void dma_memory_address_config(uint32_t dma, dma_channel_enum channel, uint32_t address) {}
void dma_transfer_number_config(uint32_t dma, dma_channel_enum channel, uint32_t number) { transfer_size = number; }
void spi_dma_enable(uint32_t spi, uint8_t dma) {}
void spi_dma_disable(uint32_t spi, uint8_t dma) {}
FlagStatus spi_i2s_flag_get(uint32_t spi, uint32_t flag) { return RESET; }
void gpio_init(uint32_t gpio, uint32_t mode, uint32_t speed, uint32_t pins) { pin_mode = mode; }

int main(void)
{
    uint32_t pixels[WS2812_STRIP0_LED_COUNT] = {0};
    ws2812_frame_t frame = { pixels, WS2812_STRIP0_LED_COUNT };
    uint8_t snapshot[WS2812_DMA_TX_BUFFER0_BYTES];
    uint8_t expected[WS2812_ENCODED_BYTES_PER_LED];
    pixels[0] = 0x123456U;
    assert(ws2812_driver_submit(WS2812_STRIP_0, &frame));
    assert(starts == 1 && transfer_size == sizeof(snapshot));
    assert(pin_mode == GPIO_MODE_AF_PP);
    ws2812_driver_encode_pixel(expected, 0, 0x12, 0x34, 0x56);
    assert(memcmp(expected, ws2812_devices[0].dma_tx_buffer, sizeof(expected)) == 0);
    memcpy(snapshot, ws2812_devices[0].dma_tx_buffer, sizeof(snapshot));
    for (unsigned i = WS2812_STRIP0_FRAME_BYTES; i < sizeof(snapshot); i++) assert(snapshot[i] == 0);

    pixels[0] = 0xFEDCBAU;
    assert(!ws2812_driver_submit(WS2812_STRIP_0, &frame));
    ws2812_driver_dma_irq(); /* Incomplete IRQ must not release ownership. */
    assert(ws2812_devices[0].transfer_started && starts == 1);
    assert(memcmp(snapshot, ws2812_devices[0].dma_tx_buffer, sizeof(snapshot)) == 0);
    completed = SET; /* Completion pending before IRQ: submit must finish the previous transfer first. */
    assert(ws2812_driver_submit(WS2812_STRIP_0, &frame));
    assert(starts == 2 && ws2812_devices[0].transfer_started);
    ws2812_driver_encode_pixel(expected, 0, 0xFE, 0xDC, 0xBA);
    assert(memcmp(expected, ws2812_devices[0].dma_tx_buffer, sizeof(expected)) == 0);
    completed = SET;
    ws2812_driver_dma_irq();
    assert(!ws2812_devices[0].transfer_started && pin_mode == GPIO_MODE_IPU);
    memcpy(snapshot, ws2812_devices[0].dma_tx_buffer, sizeof(snapshot));
    assert(!ws2812_driver_submit(WS2812_STRIP_COUNT, &frame));
    assert(!ws2812_driver_submit((ws2812_strip_e)-1, &frame));
    assert(!ws2812_driver_submit(WS2812_STRIP_0, NULL));
    frame.led_count--;
    assert(!ws2812_driver_submit(WS2812_STRIP_0, &frame));
    frame.led_count = WS2812_STRIP0_LED_COUNT;
    frame.pixels = NULL;
    assert(!ws2812_driver_submit(WS2812_STRIP_0, &frame));
    assert(starts == 2 && memcmp(snapshot, ws2812_devices[0].dma_tx_buffer, sizeof(snapshot)) == 0);
    return 0;
}
