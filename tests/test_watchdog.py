from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUBS = r'''
#ifndef WATCHDOG_TEST_STUBS_H
#define WATCHDOG_TEST_STUBS_H
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef void *QueueHandle_t;
typedef int my_message_node_t;
#define pdFALSE 0
#define pdTRUE 1
#define SUCCESS 1
#define FWDGT_PSC_DIV64 4
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP 0
static unsigned feeds, assertions, configured;
static int config_result = SUCCESS;
#define configASSERT(x) do { if (!(x)) ++assertions; } while (0)
static int fwdgt_config(uint16_t reload, uint8_t divider) {
    assert(reload == 1249 && divider == FWDGT_PSC_DIV64);
    ++configured;
    return config_result;
}
static void fwdgt_counter_reload(void) { ++feeds; }
static void SEGGER_RTT_Init(void) {}
static void SEGGER_RTT_SetFlagsUpBuffer(unsigned index, unsigned flags) {}
static void SEGGER_RTT_printf(unsigned index, const char *format, ...) {}
static QueueHandle_t xQueueCreate(unsigned count, size_t size) { return &feeds; }
static void rx_frame_init(void) {}
static void board_init(void) {}
#endif
'''
CHECK = r'''
#include "freertos_hooks.c"
int main(int argc, char **argv) {
    /* Idle must not feed an uninitialized watchdog. */
    vApplicationIdleHook();
    assert(feeds == 0);
    if (argc > 1) {
        config_result = 0;
        vApplicationDaemonTaskStartupHook();
        vApplicationIdleHook();
        assert(configured == 1 && assertions == 1 && feeds == 0);
        return 0;
    }
    vApplicationDaemonTaskStartupHook();
    assert(configured == 1 && assertions == 0);
    assert(feeds == 0);
    /* Every idle invocation must reload, without waiting for a tick. */
    vApplicationIdleHook();
    assert(feeds == 1);
    vApplicationIdleHook();
    assert(feeds == 2);
    vApplicationIdleHook();
    assert(feeds == 3);
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    for name in ("FreeRTOS.h", "SEGGER_RTT.h", "board_config.h", "comm_message.h",
                 "comm_dispatch.h", "comm_receive.h", "task.h", "gd32f10x_fwdgt.h"):
        (directory / name).write_text(STUBS)
    (directory / "check.c").write_text(CHECK)
    executable = directory / "check"
    subprocess.run(["cc", "-std=c99", f"-I{directory}",
                    f"-I{ROOT / 'Application/Runtime'}", str(directory / "check.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    subprocess.run([str(executable), "config-failure"], check=True)
print("PASS: watchdog 2 s configuration, feed on every idle hook and configuration failure")
