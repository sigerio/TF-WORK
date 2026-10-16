from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECK = r'''
#include <assert.h>
#include <string.h>
#include "uart_transport.h"
#include "gd32f10x_usart.h"
static uint32_t mock_status[2], remaining[7], memory_address[7];
static bool enabled[7];
static unsigned irq_enables;
static unsigned port_index(uint32_t p) {
    assert(p == USART0 || p == USART1);
    return p == USART0 ? 0 : 1;
}
#undef USART_STAT
#define USART_STAT(p) mock_status[port_index(p)]
#include "uart_transport.c"
#include "board_config.c"
#include "gd32f10x_it.c"
void dma_channel_disable(uint32_t p, dma_channel_enum c) { assert(p == DMA0); enabled[c] = false; }
void dma_channel_enable(uint32_t p, dma_channel_enum c) { assert(p == DMA0); enabled[c] = true; }
uint32_t dma_transfer_number_get(uint32_t p, dma_channel_enum c) { assert(p == DMA0); return remaining[c]; }
void dma_transfer_number_config(uint32_t p, dma_channel_enum c, uint32_t n) { assert(p == DMA0); remaining[c] = n; }
void dma_memory_address_config(uint32_t p, dma_channel_enum c, uint32_t a) { assert(p == DMA0); memory_address[c] = a; }
uint16_t usart_data_receive(uint32_t p) { mock_status[port_index(p)] = 0; return 0; }
void rcu_periph_clock_enable(rcu_periph_enum p) { assert(p == RCU_DMA0); }
void dma_deinit(uint32_t p, dma_channel_enum c) { assert(p == DMA0); enabled[c] = false; }
void dma_struct_para_init(dma_parameter_struct *p) { memset(p, 0, sizeof(*p)); }
void dma_init(uint32_t p, dma_channel_enum c, dma_parameter_struct *v) {
    assert(p == DMA0 && c >= DMA_CH3 && c <= DMA_CH6);
    bool rx = c == DMA_CH4 || c == DMA_CH5;
    unsigned port = c <= DMA_CH4 ? 0 : 1;
    assert(v->direction == (rx ? DMA_PERIPHERAL_TO_MEMORY : DMA_MEMORY_TO_PERIPHERAL));
    assert(v->periph_addr == board_usart_configs[port].peripheral + 4);
    assert(v->memory_addr == (uint32_t)(rx ? uart_dma[port].rx_buffer : uart_dma[port].tx_buffer));
    assert(v->number == (rx ? UART_DMA_RX_SIZE : 0));
    memory_address[c] = v->memory_addr;
    remaining[c] = v->number;
}
void dma_circulation_disable(uint32_t p, dma_channel_enum c) { assert(p == DMA0); }
void usart_dma_receive_config(uint32_t p, uint8_t v) { port_index(p); assert(v == USART_RECEIVE_DMA_ENABLE); }
void usart_interrupt_enable(uint32_t p, uint32_t v) { port_index(p); assert(v == USART_INT_IDLE || v == USART_INT_ERR); }
void nvic_irq_enable(IRQn_Type irq, uint8_t priority, uint8_t sub) {
    assert((irq == USART0_IRQn || irq == USART1_IRQn) && priority == 5 && sub == 0);
    ++irq_enables;
}
static void receive(board_usart_id_e port, const uint8_t *data, unsigned n) {
    dma_channel_enum ch = BOARD_DMA_CH(board_usart_configs[port].rx_dma)->channel;
    unsigned offset = UART_DMA_RX_SIZE - remaining[ch];
    assert(enabled[ch] && n <= remaining[ch]);
    memcpy(uart_dma[port].rx_buffer + offset, data, n);
    remaining[ch] -= n;
}
static void idle(board_usart_id_e port) {
    mock_status[port] |= USART_STAT_IDLEF;
    board_usart_dma_rx_idle_irq_handler(port);
    dma_channel_enum ch = BOARD_DMA_CH(board_usart_configs[port].rx_dma)->channel;
    assert(enabled[ch] && remaining[ch] == UART_DMA_RX_SIZE);
    assert(memory_address[ch] == (uint32_t)uart_dma[port].rx_buffer);
}
int main(void) {
    uint8_t frames[2][8] = {{0x0A,4,2,0,7,0,0,0}, {0x14,4,0x31,0,0,1,0,0}};
    uint8_t output[256], large[257];
    uint16_t length;
    assert(BOARD_USART_MT80 == 0 && BOARD_USART_MAIN == 1);
    assert(board_usart_configs[BOARD_USART_MT80].baudrate == 115200);
    assert(board_usart_configs[BOARD_USART_MAIN].baudrate == 9600);
    assert(board_usart_configs[BOARD_USART_SOLAR].peripheral == UART4);
    assert(BOARD_PIN_PORT(BOARD_PIN_UART4_TX) == GPIOC && BOARD_PIN_PIN(BOARD_PIN_UART4_TX) == GPIO_PIN_12);
    assert(BOARD_PIN_PORT(BOARD_PIN_UART4_RX) == GPIOD && BOARD_PIN_PIN(BOARD_PIN_UART4_RX) == GPIO_PIN_2);
    assert(BOARD_PIN_PORT(BOARD_PIN_RS485_SOLAR_DIR) == GPIOB && BOARD_PIN_PIN(BOARD_PIN_RS485_SOLAR_DIR) == GPIO_PIN_13);
    assert(board_gpio_configs[BOARD_PIN_BEEP].initial_level == 0);
    for (unsigned port = 0; port < 2; ++port) {
        board_usart_dma_tx_init(port);
        board_usart_dma_rx_init(port, port == 0 ? USART0_IRQn : USART1_IRQn);
        receive(port, frames[port], 8);
        idle(port);
    }
    assert(irq_enables == 2);
    /* Both ports may have a pending frame at once. */
    for (unsigned port = 0; port < 2; ++port) {
        assert(board_usart_dma_rx_fetch(port, output, &length));
        assert(length == 8 && memcmp(output, frames[port], 8) == 0);
        assert(!board_usart_dma_rx_fetch(port, output, &length));
        idle(port); /* Empty IDLE creates no frame. */
        assert(!board_usart_dma_rx_fetch(port, output, &length));
        receive(port, frames[port], 8); idle(port);
        receive(port, frames[1-port], 8); idle(port);
        assert(board_usart_dma_rx_fetch(port, output, &length));
        assert(memcmp(output, frames[port], 8) == 0); /* Keep earlier frame. */
        memset(large, 0x5A, sizeof(large));
        receive(port, large, 256); idle(port);
        assert(board_usart_dma_rx_fetch(port, output, &length) && length == 256);
        assert(memcmp(output, large, 256) == 0);
        receive(port, large, 257); idle(port);
        assert(!board_usart_dma_rx_fetch(port, output, &length));
        receive(port, frames[port], 8);
        mock_status[port] = USART_STAT_ORERR;
        board_usart_dma_rx_idle_irq_handler(port);
        assert(uart_dma[port].rx_invalid);
        idle(port);
        assert(!board_usart_dma_rx_fetch(port, output, &length));
        receive(port, frames[port], 8); idle(port);
        assert(board_usart_dma_rx_fetch(port, output, &length));
    }
    assert(!board_usart_dma_rx_fetch(BOARD_USART_SOLAR, output, &length));
    assert(!board_usart_dma_rx_fetch(BOARD_USART_MT80, NULL, &length));
    assert(!board_usart_dma_rx_fetch(BOARD_USART_MAIN, output, NULL));
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    (directory / "FreeRTOS.h").write_text("#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n")
    (directory / "task.h").write_text("")
    (directory / "check.c").write_text(CHECK)
    includes = [directory, ROOT / "Platform/Communication", ROOT / "Platform/Board", ROOT / "Platform/Interrupts",
                ROOT / "ThirdParty/GD32F10x/CMSIS", ROOT / "ThirdParty/GD32F10x/StandardPeripheralLibrary/Include"]
    subprocess.run(["cc", "-std=c99", "-DGD32F10X_HD", "-ffunction-sections", "-fdata-sections",
                    "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", *[f"-I{path}" for path in includes],
                    str(directory / "check.c"), "-Wl,--gc-sections", "-o", str(directory / "check")], check=True)
    subprocess.run([str(directory / "check")], check=True)
print("PASS: DMA0 CH3/4 and CH5/6 initialization, isolated RX, boundaries, errors and UART4 pins")
