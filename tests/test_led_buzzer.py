from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECK = r'''
#include <assert.h>
#include <setjmp.h>
#include "task_scheduler.c"
const board_gpio_config_t board_gpio_configs[BOARD_PIN_COUNT] = {
    [BOARD_PIN_LED] = {.port = GPIOC, .pin = GPIO_PIN_7},
    [BOARD_PIN_BEEP] = {.port = GPIOC, .pin = GPIO_PIN_1}
};
static jmp_buf finished;
static unsigned iteration, writes;
void gpio_bit_write(uint32_t port, uint32_t pin, bit_status value) {
    assert(port == GPIOC && pin == GPIO_PIN_7); /* No buzzer writes. */
    assert(value == (iteration % 2 ? RESET : SET));
    ++writes;
}
void vTaskDelay(TickType_t ticks) {
    assert(ticks == 500 && writes == iteration);
    if (++iteration == 5) longjmp(finished, 1);
}
int main(void) {
    if (setjmp(finished) == 0) run_led_task(NULL);
    assert(writes == 4);
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    (directory / 'FreeRTOS.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define configMAX_PRIORITIES 8U
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
#define configASSERT(x) ((void)(x))
''')
    (directory / 'task.h').write_text('''#pragma once
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
void vTaskDelete(void *task);
void vTaskStartScheduler(void);
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, unsigned stack,
                      void *arg, unsigned priority, void *handle);
''')
    (directory / 'queue.h').write_text('#pragma once\ntypedef void *QueueHandle_t;\n')
    (directory / 'check.c').write_text(CHECK)
    includes = [directory, ROOT / 'Application/Runtime', ROOT / 'Application/Communication',
                ROOT / 'Platform/Board', ROOT / 'Platform/Communication',
                ROOT / 'ThirdParty/GD32F10x/CMSIS', ROOT / 'ThirdParty/GD32F10x/StandardPeripheralLibrary/Include']
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-DGD32F10X_HD',
                    '-ffunction-sections', '-fdata-sections', *[f'-I{p}' for p in includes],
                    str(directory / 'check.c'), '-Wl,--gc-sections', '-o', str(directory / 'check')], check=True)
    subprocess.run([str(directory / 'check')], check=True, timeout=5)
print('PASS: independent 500 ms LED, no alarm/buzzer writes')
