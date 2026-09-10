from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECK = r'''
#include <assert.h>
#include <string.h>
#include "uart_transport.h"
#include "gd32f10x_usart.h"
static uint32_t status_register, dma_remaining;
static uint8_t data_register;
static bool dma_enabled;
static uint32_t dma_address;
static uint32_t read_data(uint32_t peripheral) {
    assert(peripheral == USART1);
    status_register &= ~(USART_STAT_IDLEF | USART_STAT_RBNE);
    return data_register;
}
#undef USART_STAT
#undef USART_DATA
#define USART_STAT(peripheral) status_register
#define USART_DATA(peripheral) read_data(peripheral)
#include "uart_transport.c"
const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    [BOARD_USART_MAIN] = {.rx_dma = {BOARD_DMA_0, BOARD_DMA_CH5}}
};
const board_dma_config_t board_dma_configs[BOARD_DMA_COUNT] = {
    [BOARD_DMA_0] = {.controller = DMA0, .ch = {[BOARD_DMA_CH5] = {.channel = DMA_CH5}}}
};
static void check(uint32_t p, dma_channel_enum c) { assert(p == DMA0 && c == DMA_CH5); }
void dma_channel_disable(uint32_t p, dma_channel_enum c) { check(p,c); dma_enabled=false; }
void dma_channel_enable(uint32_t p, dma_channel_enum c) { check(p,c); dma_enabled=true; }
uint32_t dma_transfer_number_get(uint32_t p, dma_channel_enum c) { check(p,c); return dma_remaining; }
void dma_transfer_number_config(uint32_t p, dma_channel_enum c, uint32_t n) { check(p,c); dma_remaining=n; }
void dma_memory_address_config(uint32_t p, dma_channel_enum c, uint32_t a) { check(p,c); dma_address=a; }
void dma_flag_clear(uint32_t p, dma_channel_enum c, uint32_t f) { check(p,c); }
FlagStatus dma_flag_get(uint32_t p, dma_channel_enum c, uint32_t f) { check(p,c); return RESET; }
FlagStatus usart_interrupt_flag_get(uint32_t p, uint32_t f) { return (status_register & USART_STAT_IDLEF) ? SET : RESET; }
uint16_t usart_data_receive(uint32_t p) { return (uint16_t)read_data(p); }
static void receive(const uint8_t *data, unsigned n) {
    unsigned offset = UART_FRAME_MAX_SIZE - dma_remaining;
    assert(dma_enabled && n <= dma_remaining);
    memcpy(usart1_dma_rx_buffer + offset, data, n);
    dma_remaining -= n;
}
int main(void) {
    uint8_t frame[8] = {1,2,3,4,5,6,7,8}, output[UART_FRAME_MAX_SIZE];
    uint16_t length;
    dma_remaining = UART_FRAME_MAX_SIZE;
    dma_enabled = true;
    receive(frame, sizeof(frame));
    status_register = USART_STAT_IDLEF;
    board_usart_main_dma_rx_idle_irq_handler();
    assert(dma_enabled && dma_remaining == UART_FRAME_MAX_SIZE);
    assert(board_usart_main_dma_rx_fetch(output, &length));
    assert(length == sizeof(frame) && memcmp(output, frame, sizeof(frame)) == 0);
    assert(!board_usart_main_dma_rx_fetch(output, &length));
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    (directory / "FreeRTOS.h").write_text("#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n")
    (directory / "task.h").write_text("")
    (directory / "check.c").write_text(CHECK)
    includes = [directory, ROOT / "Platform/Communication", ROOT / "Platform/Board",
                ROOT / "ThirdParty/GD32F10x/CMSIS", ROOT / "ThirdParty/GD32F10x/StandardPeripheralLibrary/Include"]
    subprocess.run(["cc", "-std=c99", "-DGD32F10X_HD", "-ffunction-sections", "-fdata-sections",
                    "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", *[f"-I{path}" for path in includes],
                    str(directory / "check.c"), "-Wl,--gc-sections", "-o", str(directory / "check")], check=True)
    subprocess.run([str(directory / "check")], check=True)
print("PASS: USART1 DMA + hardware IDLE vendor sequence")
