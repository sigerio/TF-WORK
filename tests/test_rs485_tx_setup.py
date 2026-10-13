from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECK = r'''
#include <assert.h>
#include "uart_transport.c"
const board_gpio_config_t board_gpio_configs[BOARD_PIN_COUNT] = {
    [BOARD_PIN_RS485_MAIN_DIR] = {.port = GPIOA, .pin = GPIO_PIN_1}
};
const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    [BOARD_USART_MAIN] = {.peripheral = USART1, .baudrate = 9600,
        .direction_pin = BOARD_PIN_RS485_MAIN_DIR, .tx_dma = {BOARD_DMA_0, BOARD_DMA_CH6}},
    [BOARD_USART_TC] = {.peripheral = USART0, .baudrate = 9600, .direction_pin = BOARD_PIN_RS485_MAIN_DIR},
    [BOARD_USART_WIND] = {.peripheral = USART2, .baudrate = 9600, .direction_pin = BOARD_PIN_RS485_MAIN_DIR},
    [BOARD_USART_RAIN] = {.peripheral = UART3, .baudrate = 9600, .direction_pin = BOARD_PIN_RS485_MAIN_DIR},
    [BOARD_USART_WATER] = {.peripheral = UART4, .baudrate = 9600, .direction_pin = BOARD_PIN_RS485_MAIN_DIR}
};
const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT] = {
    [BOARD_DMA_0] = {.controller = DMA0, .ch = {[BOARD_DMA_CH6] = {.channel = DMA_CH6}}}
};
static uint32_t clock_us, direction_at, remaining;
static unsigned transmitted, rises;
static bool direction, receiver = true, tc = true, dma_active, inject_poll;
uint32_t timer_counter_read(uint32_t p) {
    assert(p == TIMER2);
    if (inject_poll) {
        /* Simulate a pending USART1 IRQ during the optocoupler setup wait. */
        inject_poll = false;
        board_usart_main_dma_tx_poll();
        inject_poll = true;
    }
    return (uint16_t)clock_us++;
}
void gpio_bit_write(uint32_t p, uint32_t pin, bit_status value) {
    assert(p == GPIOA && pin == GPIO_PIN_1);
    direction = value != RESET;
    if (direction) {
        assert(!receiver);
        direction_at = clock_us;
        ++rises;
    }
}
static void check_setup(void) {
    assert(direction && !receiver);
    assert(clock_us - direction_at >= RS485_TX_SETUP_US);
}
void usart_receive_config(uint32_t p, uint32_t value) { receiver = value == USART_RECEIVE_ENABLE; }
void usart_flag_clear(uint32_t p, usart_flag_enum f) { if (f == USART_FLAG_TC) tc = false; }
FlagStatus usart_flag_get(uint32_t p, usart_flag_enum f) {
    return f == USART_FLAG_TC ? (tc ? SET : RESET) : SET;
}
void usart_data_transmit(uint32_t p, uint16_t data) {
    check_setup();
    ++transmitted;
    tc = true;
}
void usart_interrupt_enable(uint32_t p, uint32_t f) {}
void usart_interrupt_disable(uint32_t p, uint32_t f) {}
void usart_dma_transmit_config(uint32_t p, uint8_t value) {
    if (value == USART_TRANSMIT_DMA_ENABLE) check_setup();
}
void dma_channel_enable(uint32_t p, dma_channel_enum c) { check_setup(); dma_active = true; }
void dma_channel_disable(uint32_t p, dma_channel_enum c) { dma_active = false; }
void dma_interrupt_flag_clear(uint32_t p, dma_channel_enum c, uint32_t f) {}
void dma_memory_address_config(uint32_t p, dma_channel_enum c, uint32_t a) {}
void dma_transfer_number_config(uint32_t p, dma_channel_enum c, uint32_t n) { remaining = n; }
uint32_t dma_transfer_number_get(uint32_t p, dma_channel_enum c) { return remaining; }
int main(void) {
    const uint8_t data[] = {1, 3, 0, 0};
    for (unsigned port = 0; port < BOARD_USART_COUNT; ++port) {
        /* Cross the 16-bit timer wrap on every send. */
        clock_us = 65530;
        inject_poll = true;
        unsigned previous_rises = rises;
        assert(board_usart_send((board_usart_id_e)port, data, sizeof(data)));
        inject_poll = false;
        assert(rises == previous_rises + 1);
        if (port == BOARD_USART_MAIN) {
            assert(direction && dma_active && usart1_dma_tx_busy);
            assert(!board_usart_send(BOARD_USART_MAIN, data, sizeof(data)));
            assert(rises == previous_rises + 1);
            remaining = 0;
            board_usart_main_dma_tx_poll();
            assert(direction); /* DMA done alone must not release DIR. */
            tc = true;
            board_usart_main_dma_tx_poll();
            assert(!dma_active && !usart1_dma_tx_busy);
        }
        assert(!direction && receiver);
    }
    assert(transmitted == 4 * sizeof(data));
    board_usart_config_t unisolated = {.direction_pin = BOARD_PIN_NONE};
    uint32_t before = clock_us;
    board_usart_direction_set(&unisolated, SET);
    assert(clock_us == before);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    (directory / 'FreeRTOS.h').write_text('#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n')
    (directory / 'task.h').write_text('')
    (directory / 'check.c').write_text(CHECK)
    includes = [directory, ROOT / 'Platform/Communication', ROOT / 'Platform/Board',
                ROOT / 'ThirdParty/GD32F10x/CMSIS', ROOT / 'ThirdParty/GD32F10x/StandardPeripheralLibrary/Include']
    for setup in (100, 250):
        subprocess.run(['cc', '-std=c99', '-DGD32F10X_HD', f'-DRS485_TX_SETUP_US={setup}',
                        '-ffunction-sections', '-fdata-sections', '-Wno-pointer-to-int-cast',
                        '-Wno-int-to-pointer-cast', *[f'-I{p}' for p in includes],
                        str(directory / 'check.c'), '-Wl,--gc-sections', '-o', str(directory / 'check')], check=True)
        subprocess.run([str(directory / 'check')], check=True, timeout=5)
print('PASS: all RS485 TX setup delays, timer wrap, DMA IRQ during setup and TC release')
