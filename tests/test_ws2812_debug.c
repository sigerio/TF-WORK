/* Real command parsing and effect library; capture RGB frames before hardware encoding. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gd32f10x.h"

#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#include "DEBUG/debug_console.c"
#include "CONTROL/ws2812/ws2812_effect.h"

static uint32_t work_colors[24];
static uint32_t sent_colors[24];
static unsigned led_count = MODULE_WS2812_STRIP0_LED_COUNT;
static unsigned copies, sends;

uint32_t get_strip_led_count(module_ws2812_id_e strip)
{
    assert(strip == MODULE_WS2812_STRIP_0);
    return led_count;
}
void module_ws2812_clear_work_frame(void) { memset(work_colors, 0, sizeof(work_colors)); }
void module_ws2812_set_rgb_range(module_ws2812_id_e strip, uint32_t first, uint32_t last, uint32_t color)
{
    assert(strip == MODULE_WS2812_STRIP_0 && first <= last && last < led_count);
    assert(color <= 0xFFFFFFU);
    for (uint32_t i = first; i <= last; i++) work_colors[i] = color;
}
void module_ws2812_copy_work_frame_to_dma(module_ws2812_id_e strip)
{
    assert(strip == MODULE_WS2812_STRIP_0);
    memcpy(sent_colors, work_colors, sizeof(sent_colors));
    copies++;
}
void module_ws2812_send_frame_dma(module_ws2812_id_e strip)
{
    assert(strip == MODULE_WS2812_STRIP_0 && copies == sends + 1);
    sends++;
}
static void tick(void)
{
    unsigned before = sends;
    module_ws2812_status_update(MODULE_WS2812_PERIOD_MS);
    assert(sends == before + 1);
}
static void assert_color(uint32_t color)
{
    for (unsigned i = 0; i < led_count; i++) assert(work_colors[i] == color);
}
static void basic(ws2812_basic_effect_e effect, unsigned elapsed)
{
    ws2812_render_basic_effect(MODULE_WS2812_STRIP_0, effect, elapsed);
}

static void check_basic_effects(void)
{
    /* Preserve the original six effects, including exact timing and full-scale breathing. */
    for (unsigned elapsed = 0; elapsed < 2200; elapsed += 50) {
        basic(WS2812_BASIC_GREEN_FLASH_THEN_STEADY, elapsed);
        assert_color(elapsed >= 1200 || elapsed % 400 < 200 ? 0x00FF00U : 0);
    }
    basic(WS2812_BASIC_GREEN_FLASH_THEN_STEADY, 0);
    assert_color(0x00FF00U);
    basic(WS2812_BASIC_YELLOW_STEADY, 0);
    assert_color(0xFFFF00U);
    for (unsigned elapsed = 0; elapsed < 3100; elapsed += 50) {
        basic(WS2812_BASIC_RED_FLASH, elapsed);
        assert_color(elapsed % 1000 < 700 ? 0xFF0000U : 0);
    }
    basic(WS2812_BASIC_RED_STEADY, 0);
    assert_color(0xFF0000U);
    unsigned previous_level = 0;
    for (unsigned elapsed = 0; elapsed <= 8500; elapsed += 50) {
        unsigned phase = elapsed % 4000;
        basic(WS2812_BASIC_WHITE_BREATHING, elapsed);
        unsigned level = work_colors[0] & 0xFFU;
        assert_color(WS2812_RGB(level, level, level));
        if (phase == 0) assert(level == 0);
        else if (phase <= 2000) assert(level > previous_level);
        else assert(level < previous_level);
        if (phase == 2000) assert(level == 255);
        if (phase == 1000 || phase == 3000) assert(level == 127);
        previous_level = level;
    }
    for (unsigned frame = 0; frame < 30; frame++) {
        unsigned head = (frame / 2) % led_count;
        unsigned previous = 256;
        basic(WS2812_BASIC_WHITE_TRAILING, frame * 50);
        for (unsigned distance = 0; distance < led_count; distance++) {
            unsigned index = (head + led_count - distance) % led_count;
            unsigned level = work_colors[index] & 0xFFU;
            assert(work_colors[index] == WS2812_RGB(level, level, level));
            if (distance < 6) assert(level > 0 && level < previous);
            else assert(level == 0);
            previous = level;
        }
    }
}

static void (*const effects[])(module_ws2812_id_e, uint32_t) = {
    ws2812_effect_reactor_heartbeat, ws2812_effect_dual_rotor, ws2812_effect_heavy_scan,
    ws2812_effect_shockwave, ws2812_effect_cold_plasma, ws2812_effect_warp_charge,
};

static void check_new_effects(void)
{
    const unsigned counts[] = {1, 2, 12, 24};
    const unsigned periods[] = {3600, 3200, 4800, 2800, 21600, 6000};
    uint32_t first[24];
    uint32_t previous[24];

    for (unsigned size = 0; size < sizeof(counts) / sizeof(counts[0]); size++) {
        led_count = counts[size];
        for (unsigned effect = 0; effect < 6; effect++) {
            effects[effect](MODULE_WS2812_STRIP_0, 0);
            memcpy(first, work_colors, sizeof(first));
            effects[effect](MODULE_WS2812_STRIP_0, periods[effect]);
            assert(memcmp(first, work_colors, led_count * sizeof(uint32_t)) == 0);
            unsigned changes = 0;
            for (unsigned elapsed = 0; elapsed < 12000; elapsed += 50) {
                memcpy(previous, work_colors, sizeof(previous));
                memset(work_colors, 0xA5, sizeof(work_colors));
                effects[effect](MODULE_WS2812_STRIP_0, elapsed);
                changes += memcmp(previous, work_colors, led_count * sizeof(uint32_t)) != 0;
                for (unsigned i = 0; i < led_count; i++) assert(work_colors[i] <= 0xFFFFFFU);
                for (unsigned i = led_count; i < 24; i++) assert(work_colors[i] == 0xA5A5A5A5U);
                if (effect == 0 || effect == 3 || effect == 5) {
                    for (unsigned i = 0; i < led_count; i++) assert(work_colors[i] == work_colors[led_count - 1 - i]);
                }
            }
            assert(changes > 20);
            effects[effect](MODULE_WS2812_STRIP_0, UINT32_MAX);
            memcpy(previous, work_colors, sizeof(previous));
            effects[effect]((module_ws2812_id_e)-1, 0);
            effects[effect](MODULE_WS2812_ID_COUNT, 0);
            assert(memcmp(previous, work_colors, sizeof(previous)) == 0);
        }
    }
    led_count = MODULE_WS2812_STRIP0_LED_COUNT;
    ws2812_effect_warp_charge(MODULE_WS2812_STRIP_0, 3400);
    assert_color(0xC8EBFFU);
    ws2812_effect_warp_charge(MODULE_WS2812_STRIP_0, 5000);
    assert_color(0x000206U);
    ws2812_effect_heavy_scan(MODULE_WS2812_STRIP_0, 0);
    memcpy(first, work_colors, sizeof(first));
    ws2812_effect_heavy_scan(MODULE_WS2812_STRIP_0, 2400);
    for (unsigned i = 0; i < led_count; i++) assert(first[i] == work_colors[led_count - 1 - i]);
}

static void check_commands(void)
{
    const char *invalid[] = {"WSE:7", "WSE:255", "WSE:-1", "WSE:", "WSE:2x"};
    uint32_t first[24];

    assert(debug_console_apply_command("AC:1"));
    assert(debug_console_apply_command("WETH:1"));
    assert(debug_console_apply_command("IEC:1"));
    for (unsigned effect = 0; effect < 6; effect++) {
        char command[] = "WSE:1";
        command[4] = (char)('1' + effect);
        assert(debug_console_apply_command(command));
        tick();
        memcpy(first, sent_colors, sizeof(first));
        effects[effect](MODULE_WS2812_STRIP_0, 0);
        assert(memcmp(first, work_colors, led_count * sizeof(uint32_t)) == 0);
        for (unsigned i = 0; i < 37; i++) tick();
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) assert(!debug_console_apply_command(invalid[i]));
        assert(!module_ws2812_debug_select((ws2812_debug_effect_e)-1));
        assert(!module_ws2812_debug_select((ws2812_debug_effect_e)7));
        assert(debug_ctrl.bits.wse == effect + 1);
        tick();
        effects[effect](MODULE_WS2812_STRIP_0, 1900);
        assert(memcmp(sent_colors, work_colors, led_count * sizeof(uint32_t)) == 0);
        assert(debug_console_apply_command(command));
        tick();
        assert(memcmp(first, sent_colors, led_count * sizeof(uint32_t)) == 0);
        assert(module_ws2812_effect_is_active());
    }
    assert(debug_ctrl.bits.ac && debug_ctrl.bits.weth && debug_ctrl.bits.iec);
    ringbuf_init(&s_rx_fifo, s_rx_fifo_buf, sizeof(s_rx_fifo_buf));
    const char *commands = "WSE:0\r\nWSE:2\r\n";
    assert(ringbuf_write(&s_rx_fifo, (const uint8_t *)commands, strlen(commands)) == strlen(commands));
    debug_console_task();
    tick();
    effects[1](MODULE_WS2812_STRIP_0, 0);
    assert(memcmp(sent_colors, work_colors, led_count * sizeof(uint32_t)) == 0);

    set_door_running_in(E_DOOR_RUN_IN_ACTION);
    assert(debug_console_apply_command("WSE:3"));
    tick();
    effects[2](MODULE_WS2812_STRIP_0, 0);
    assert(memcmp(sent_colors, work_colors, led_count * sizeof(uint32_t)) == 0);
    assert(debug_console_apply_command("WSE:0"));
    tick();
    for (unsigned i = 0; i < 8; i++) assert((sent_colors[i] & 0xFF0000U) != 0);
    for (unsigned i = 8; i < led_count; i++) assert(sent_colors[i] == 0);
    set_door_running_in(E_DOOR_RUN_IDLE);
    tick();
    assert_color(0);
    assert(!module_ws2812_effect_is_active());
}

int main(int argc, char **argv)
{
    assert(MODULE_WS2812_PERIOD_MS == 50);
    check_basic_effects();
    check_new_effects();
    check_commands();
    /* Optional preview exports actual C-rendered frames, not a second animation implementation. */
    if (argc == 2 && strcmp(argv[1], "--preview") == 0) {
        puts("[");
        for (unsigned effect = 0; effect < 6; effect++) {
            puts(effect ? ",[" : "[");
            for (unsigned elapsed = 0; elapsed < 24000; elapsed += 50) {
                effects[effect](MODULE_WS2812_STRIP_0, elapsed);
                printf(elapsed ? ",[" : "[");
                for (unsigned i = 0; i < led_count; i++) printf(i ? ",%u" : "%u", (unsigned)work_colors[i]);
                putchar(']');
            }
            puts("]");
        }
        puts("]");
    }
    return 0;
}
