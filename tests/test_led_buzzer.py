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
static unsigned iteration, reads, writes;
static uint32_t supplied_fault, time_base;
static bool led, beep, transitions;
TickType_t xTaskGetTickCount(void) { return time_base + iteration * 10U; }
static uint32_t transition_fault(void) {
    if (iteration < 100) return 2;
    if (iteration < 150) return 8;
    if (iteration < 160) return 0;
    if (iteration < 170) return 2;
    if (iteration < 420) return 8;
    if (iteration < 460) return 1U << 14; /* Water goes offline: hold the alarm. */
    if (iteration < 500) return 1U << 19; /* Invalid water data also holds it. */
    if (iteration < 550) return 0;
    if (iteration < 560) return 8; /* Recovery interrupted by renewed water. */
    if (iteration < 800) return 0;
    if (iteration < 810) return 8;
    if (iteration < 830) return 9; /* ESTOP overrides water, silently. */
    if (iteration < 1100) return 8;
    if (iteration < 1150) return 10; /* Limit overrides water. */
    return 0;
}
bool weather_fault_get(weather_fault_u *fault) {
    ++reads;
    if (iteration < (transitions ? 10U : 60U)) return false;
    fault->raw = transitions ? transition_fault() : supplied_fault;
    return true;
}
void gpio_bit_write(uint32_t port, uint32_t pin, bit_status value) {
    assert(port == GPIOC);
    if (pin == GPIO_PIN_7) led = value == RESET;
    else {
        assert(pin == GPIO_PIN_1);
        beep = value == SET;
        ++writes;
    }
}
static bool double_pulse(unsigned steps) {
    unsigned phase = steps % 150;
    return phase < 15 || (phase >= 30 && phase < 45);
}
static bool water_pulse(unsigned steps) { return steps % 100 < 50; }
static bool expected_beep(void) {
    if (transitions) {
        if (iteration < 10) return false;
        if (iteration < 370) return double_pulse(iteration - 10);
        if (iteration < 760) return water_pulse(iteration - 370);
        if (iteration < 800) return false;
        if (iteration < 810) return water_pulse(iteration - 800);
        if (iteration < 1030) return false;
        if (iteration < 1100) return water_pulse(iteration - 1030);
        if (iteration < 1350) return double_pulse(iteration - 1100);
        return false;
    }
    if (iteration < 60 || (supplied_fault & 1U)) return false;
    if (supplied_fault & 6U) return double_pulse(iteration - 60);
    if (supplied_fault & 8U) return water_pulse(iteration - 60);
    return false;
}
void vTaskDelay(TickType_t ticks) {
    assert(ticks == 10);
    assert(led == ((iteration / 50) % 2 != 0));
    assert(beep == expected_beep());
    assert(reads == iteration + 1 && writes == iteration + 1);
    if (++iteration == (transitions ? 1400U : 380U)) longjmp(finished, 1);
}
static void check(uint32_t raw) {
    supplied_fault = raw;
    iteration = reads = writes = 0;
    led = false;
    beep = true;
    if (setjmp(finished) == 0) run_led_task(NULL);
}
int main(void) {
    check(0);
    for (unsigned bit = 0; bit < 32; ++bit) check(1UL << bit);
    check(3);
    check(5);
    check(9);
    check(10);
    check(0xFFFFFFFFU);
    transitions = true;
    check(0);
    time_base = UINT32_MAX - 1000U;
    check(0); /* Recovery and pulse timing must survive system tick wrap. */
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
print('PASS: master alarm rhythms, priority, 2 s recovery, water hold, independent LED and tick wrap')
