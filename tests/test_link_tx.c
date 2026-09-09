/* Run with python3 tests/check_app_safety.py. Real link code and board mappings, mocked peripherals. */
#include <assert.h>
#include "gd32f10x.h"

/* Host-only replacements for Cortex-M interrupt instructions in unused receive paths. */
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#include "COMM/link/com_link.c"

static bool has_direction, transfer_complete;
static unsigned phase, bytes_sent, delay_calls;
static const board_gpio_config_t *direction_pin;

void gpio_bit_write(uint32_t port, uint32_t pin, bit_status value)
{
    assert(has_direction && port == direction_pin->port && pin == direction_pin->pin);
    if (value == SET) {
        assert(phase == 0);
        phase = 1;
    } else {
        assert(phase == 3 && transfer_complete);
        phase = 4;
    }
}
void board_delay_us(uint32_t us)
{
    assert(has_direction && phase == 1 && us == BOARD_RS485_TX_SETUP_US);
    phase = 2;
    delay_calls++;
}
static void assert_ready_to_send(void)
{
    assert(phase == (has_direction ? 2U : 0U) || phase == 3);
    phase = 3;
}
void dma_channel_enable(uint32_t dma, dma_channel_enum channel) { assert_ready_to_send(); }
void dma_channel_disable(uint32_t dma, dma_channel_enum channel) {}
void dma_periph_address_config(uint32_t dma, dma_channel_enum channel, uint32_t address) {}
void dma_memory_address_config(uint32_t dma, dma_channel_enum channel, uint32_t address) {}
void dma_transfer_number_config(uint32_t dma, dma_channel_enum channel, uint32_t number) {}
void dma_flag_clear(uint32_t dma, dma_channel_enum channel, uint32_t flag) {}
FlagStatus dma_flag_get(uint32_t dma, dma_channel_enum channel, uint32_t flag) { return SET; }
void usart_dma_transmit_config(uint32_t usart, uint8_t state) {}
void usart_flag_clear(uint32_t usart, uint32_t flag) {}
FlagStatus usart_flag_get(uint32_t usart, uint32_t flag)
{
    return flag == USART_FLAG_TC && !transfer_complete ? RESET : SET;
}
void usart_data_transmit(uint32_t usart, uint16_t data) { assert_ready_to_send(); bytes_sent++; }

static void check_tx(board_usart_id_e usart, bool polling)
{
    uint8_t buffer[8] = {0};
    board_usart_config_t config = board_usart_configs[usart];
    com_link_resource_t resource = {0};
    com_link_t link = {0};

    if (polling) config.tx_dma.dma = BOARD_DMA_COUNT;
    resource.usart = &config;
    resource.tx_dma_buf = buffer;
    resource.tx_dma_size = sizeof(buffer);
    link.res = &resource;
    link.initialized = true;
    has_direction = config.direction_pin != BOARD_PIN_NONE;
    direction_pin = has_direction ? &board_gpio_configs[config.direction_pin] : NULL;
    phase = bytes_sent = delay_calls = 0;
    transfer_complete = polling;
    assert(!com_link_tx_start(NULL, 8));
    assert(!com_link_tx_start(&link, 0));
    assert(!com_link_tx_start(&link, 9));
    link.tx_busy = true;
    assert(!com_link_tx_start(&link, 8));
    link.tx_busy = false;
    assert(phase == 0 && delay_calls == 0);

    assert(com_link_tx_start(&link, 8));
    assert(delay_calls == (has_direction ? 1U : 0U));
    if (polling) {
        assert(bytes_sent == 8 && !link.tx_busy);
    } else {
        assert(link.tx_busy && phase == 3);
        assert(!com_link_tx_start(&link, 8));
        com_link_tx_service(&link);
        assert(link.tx_busy && phase == 3); /* DMA done alone must not release direction before USART TC. */
        transfer_complete = true;
        com_link_tx_service(&link);
        assert(!link.tx_busy);
    }
    assert(phase == (has_direction ? 4U : 3U));
}

int main(void)
{
    check_tx(BOARD_USART_WEATHER, false);
    check_tx(BOARD_USART_AC, false);
    check_tx(BOARD_USART_MAINBOARD, false);
    check_tx(BOARD_USART_WEATHER, true);
    check_tx(BOARD_USART_MAINBOARD, true);
    return 0;
}
