/* Real command parsing and effect library; capture RGB frames before hardware encoding. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gd32f10x.h"

#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#include "DEBUG/debug_console.c"
#include "CONTROL/ws2812/effects/ws2812_basic.h"
#include "CONTROL/ws2812/effects/ws2812_mechanical.h"
#include "CONTROL/ws2812/ws2812_frame.h"
#include "CONTROL/ws2812/driver/ws2812_driver.h"
#include "APP/app_fault.h"
#include "COMM/modbus/modbus_master.h"

static app_fault_u faults;
app_fault_u app_fault_get(void) { return faults; }
static unsigned fault_debug_requests;
static uint32_t fault_debug_mask;
void modbus_master_set_weather_fault_debug(uint32_t mask) { fault_debug_mask = mask; fault_debug_requests++; }

static uint32_t work_colors[64];
static uint32_t sent_colors[64];
static ws2812_frame_t frame = { work_colors, WS2812_STRIP0_LED_COUNT };
static unsigned sends;

bool ws2812_driver_submit(ws2812_strip_e strip, const ws2812_frame_t *submitted)
{
    assert(strip == WS2812_STRIP_0 && submitted->led_count == WS2812_STRIP0_LED_COUNT);
    memcpy(sent_colors, submitted->pixels, submitted->led_count * sizeof(uint32_t));
    sends++;
    return true;
}
static void tick(void)
{
    unsigned before = sends;
    ws2812_app_update(WS2812_APP_PERIOD_MS);
    assert(sends == before + 1);
}
static void assert_color(uint32_t color)
{
    for (unsigned i = 0; i < frame.led_count; i++) assert(work_colors[i] == color);
}
static void basic(ws2812_basic_effect_e effect, unsigned elapsed)
{
    ws2812_render_basic_effect(&frame, effect, elapsed);
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
    for (unsigned step = 0; step < 30; step++) {
        unsigned head = (step / 2) % frame.led_count;
        unsigned previous = 256;
        basic(WS2812_BASIC_WHITE_TRAILING, step * 50);
        for (unsigned distance = 0; distance < frame.led_count; distance++) {
            unsigned index = (head + frame.led_count - distance) % frame.led_count;
            unsigned level = work_colors[index] & 0xFFU;
            assert(work_colors[index] == WS2812_RGB(level, level, level));
            if (distance < 6) assert(level > 0 && level < previous);
            else assert(level == 0);
            previous = level;
        }
    }
}

static void (*const effects[])(ws2812_frame_t *, uint32_t) = {
    ws2812_effect_reactor_heartbeat, ws2812_effect_dual_rotor, ws2812_effect_heavy_scan,
    ws2812_effect_shockwave, ws2812_effect_cold_plasma, ws2812_effect_warp_charge,
};

static void check_new_effects(void)
{
    const unsigned counts[] = {1, 2, 3, 6, 11, 12, 13, 24, 60};
    const unsigned periods[] = {3600, 3200, 4800, 2800, 21600, 6000};
    uint32_t first[64];
    uint32_t previous[64];

    for (unsigned size = 0; size < sizeof(counts) / sizeof(counts[0]); size++) {
        frame.led_count = counts[size];
        for (unsigned effect = 0; effect < 6; effect++) {
            effects[effect](&frame, 0);
            memcpy(first, work_colors, sizeof(first));
            effects[effect](&frame, periods[effect]);
            assert(memcmp(first, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
            unsigned changes = 0;
            for (unsigned elapsed = 0; elapsed < 12000; elapsed += 50) {
                memcpy(previous, work_colors, sizeof(previous));
                memset(work_colors, 0xA5, sizeof(work_colors));
                effects[effect](&frame, elapsed);
                changes += memcmp(previous, work_colors, frame.led_count * sizeof(uint32_t)) != 0;
                for (unsigned i = 0; i < frame.led_count; i++) assert(work_colors[i] <= 0xFFFFFFU);
                for (unsigned i = frame.led_count; i < 64; i++) assert(work_colors[i] == 0xA5A5A5A5U);
                if (effect == 0 || effect == 3 || effect == 5) {
                    for (unsigned i = 0; i < frame.led_count; i++) assert(work_colors[i] == work_colors[frame.led_count - 1 - i]);
                }
            }
            assert(changes > 20);
            effects[effect](&frame, UINT32_MAX);
            memcpy(previous, work_colors, sizeof(previous));
            effects[effect](NULL, 0);
            ws2812_frame_t invalid = { NULL, 12U };
            effects[effect](&invalid, 0);
            invalid = (ws2812_frame_t){ work_colors, 0U };
            effects[effect](&invalid, 0);
            assert(memcmp(previous, work_colors, sizeof(previous)) == 0);
        }
    }
    frame.led_count = WS2812_STRIP0_LED_COUNT;
    ws2812_effect_warp_charge(&frame, 3400);
    assert_color(0xC8EBFFU);
    ws2812_effect_warp_charge(&frame, 5000);
    assert_color(0x000206U);
    ws2812_effect_heavy_scan(&frame, 0);
    memcpy(first, work_colors, sizeof(first));
    ws2812_effect_heavy_scan(&frame, 2400);
    for (unsigned i = 0; i < frame.led_count; i++) assert(first[i] == work_colors[frame.led_count - 1 - i]);
}

static void check_frame_ownership(void)
{
    uint32_t first_pixels[8] = {0};
    uint32_t second_pixels[8] = {0};
    ws2812_frame_t first = { first_pixels, 8U };
    ws2812_frame_t second = { second_pixels, 8U };
    ws2812_trail_state_t first_trail = { .length = 3U };
    ws2812_trail_state_t second_trail = { .length = 3U };
    ws2812_breath_state_t first_breath = {0};
    ws2812_breath_state_t second_breath = {0};
    ws2812_wave_state_t first_wave = {0};
    ws2812_wave_state_t second_wave = {0};

    ws2812_frame_fill(&first, 6U, 1U, 0x123456U);
    for (unsigned i = 0; i < 8; i++) assert(first_pixels[i] == (i < 2 || i >= 6 ? 0x123456U : 0U));
    ws2812_frame_fill(&first, 8U, 1U, 0);
    assert(first_pixels[0] == 0x123456U && first_pixels[6] == 0x123456U);
    ws2812_frame_clear(&first);
    ws2812_effect_trail(&first, 0xFFFFFFU, 2U, &first_trail);
    ws2812_effect_trail(&second, 0xFFFFFFU, 2U, &second_trail);
    assert(memcmp(first_pixels, second_pixels, sizeof(first_pixels)) == 0);
    ws2812_effect_trail(&first, 0xFFFFFFU, 2U, &first_trail);
    assert(first_trail.head == 1U && second_trail.head == 0U);
    for (unsigned i = 0; i < 20; i++) ws2812_effect_breath(&first, 0U, 7U, 0xFFFFFFU, &first_breath);
    ws2812_effect_breath(&second, 0U, 7U, 0xFFFFFFU, &second_breath);
    assert(first_breath.table_index == 20U && second_breath.table_index == 1U);
    assert(first_pixels[0] != second_pixels[0]);
    ws2812_effect_wave(&first, 0xFFFFFFU, 5U, 1U, &first_wave);
    ws2812_effect_wave(&second, 0xFFFFFFU, 5U, 1U, &second_wave);
    assert(memcmp(first_pixels, second_pixels, sizeof(first_pixels)) == 0);
    ws2812_effect_wave(&first, 0xFFFFFFU, 5U, 1U, &first_wave);
    assert(first_wave.head == 2U && second_wave.head == 1U);
}

static void expected_debug(unsigned effect, unsigned elapsed)
{
    for (unsigned i = 0; i < frame.led_count; i++) {
        bool on = effect == 0;
        if (effect == 1) on = elapsed % 1500 < 150 || (elapsed % 1500 >= 300 && elapsed % 1500 < 450);
        if (effect == 2) on = frame.led_count == 1 || ((i < (frame.led_count + 1) / 2) == (elapsed % 1000 < 500));
        work_colors[i] = on ? 0xFF0000U : 0U;
    }
}

static void check_commands(void)
{
    const char *invalid[] = {"WSE:4", "WSE:5", "WSE:6", "WSE:7", "WSE:255", "WSE:-1", "WSE:", "WSE:2x"};
    uint32_t first[64];

    assert(debug_console_apply_command("AC:1"));
    assert(debug_console_apply_command("WETH:1"));
    assert(debug_console_apply_command("IEC:1"));
    for (unsigned effect = 0; effect < 3; effect++) {
        char command[] = "WSE:1";
        command[4] = (char)('1' + effect);
        unsigned requests_before = fault_debug_requests;
        uint32_t real_faults = faults.raw;
        assert(debug_console_apply_command(command));
        assert(fault_debug_requests == requests_before + 1);
        assert(fault_debug_mask == (effect == 0 ? 1U : effect == 1 ? 2U : 8U));
        assert(faults.raw == real_faults);
        tick();
        memcpy(first, sent_colors, sizeof(first));
        expected_debug(effect, 0);
        assert(memcmp(first, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
        for (unsigned i = 0; i < 37; i++) tick();
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) assert(!debug_console_apply_command(invalid[i]));
        assert(!ws2812_app_select_debug((ws2812_debug_effect_e)-1));
        assert(!ws2812_app_select_debug((ws2812_debug_effect_e)4));
        assert(fault_debug_requests == requests_before + 1); /* Invalid commands never send. */
        assert(debug_ctrl.bits.wse == effect + 1);
        tick();
        expected_debug(effect, 1900);
        assert(memcmp(sent_colors, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
        assert(debug_console_apply_command(command));
        assert(fault_debug_requests == requests_before + 2); /* Repeated command forces another notification. */
        tick();
        assert(memcmp(first, sent_colors, frame.led_count * sizeof(uint32_t)) == 0);
        assert(ws2812_app_is_active());
    }
    assert(debug_ctrl.bits.ac && debug_ctrl.bits.weth && debug_ctrl.bits.iec);
    ringbuf_init(&s_rx_fifo, s_rx_fifo_buf, sizeof(s_rx_fifo_buf));
    const char *commands = "WSE:0\r\nWSE:2\r\n";
    assert(ringbuf_write(&s_rx_fifo, (const uint8_t *)commands, strlen(commands)) == strlen(commands));
    debug_console_task();
    tick();
    expected_debug(1, 0);
    assert(memcmp(sent_colors, work_colors, frame.led_count * sizeof(uint32_t)) == 0);

    ws2812_app_set_door_state(E_DOOR_RUN_IN_ACTION);
    assert(debug_console_apply_command("WSE:3"));
    tick();
    expected_debug(2, 0);
    assert(memcmp(sent_colors, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
    assert(debug_console_apply_command("WSE:0"));
    assert(fault_debug_mask == 0U);
    tick();
    for (unsigned i = 0; i < 8; i++) assert((sent_colors[i] & 0xFF0000U) != 0);
    for (unsigned i = 8; i < frame.led_count; i++) assert(sent_colors[i] == 0);
    ws2812_app_set_door_state(E_DOOR_RUN_IDLE);
    tick();
    for (unsigned i = 0; i < WS2812_STRIP0_LED_COUNT; i++) assert(sent_colors[i] == 0);
    assert(!ws2812_app_is_active());
}

static void assert_sent_color(uint32_t color)
{
    for (unsigned i = 0; i < WS2812_STRIP0_LED_COUNT; i++) assert(sent_colors[i] == color);
}

static void check_alarm_effects(void)
{
    const unsigned counts[] = {1, 2, 3, 11, 12, 13, 60};
    for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
        frame.led_count = counts[c];
        for (unsigned ms = 0; ms < 3000; ms++) {
            memset(work_colors, 0xA5, sizeof(work_colors));
            ws2812_effect_double_flash(&frame, 0xFF0000U, ms);
            unsigned phase = ms % 1500;
            assert_color(phase < 150 || (phase >= 300 && phase < 450) ? 0xFF0000U : 0U);
            ws2812_effect_alternating_halves(&frame, 0xFF0000U, ms);
            unsigned lit = 0;
            for (unsigned i = 0; i < frame.led_count; i++) {
                bool on = frame.led_count == 1 || ((i < (frame.led_count + 1) / 2) == (ms % 1000 < 500));
                assert(work_colors[i] == (on ? 0xFF0000U : 0U));
                lit += on;
            }
            assert(lit > 0);
            for (unsigned i = frame.led_count; i < 64; i++) assert(work_colors[i] == 0xA5A5A5A5U);
        }
    }
    ws2812_effect_double_flash(NULL, 0, 0);
    ws2812_effect_alternating_halves(NULL, 0, 0);
    ws2812_frame_t invalid = {NULL, 12U};
    ws2812_effect_double_flash(&invalid, 0, 0);
    ws2812_effect_alternating_halves(&invalid, 0, 0);
    invalid = (ws2812_frame_t){work_colors, 0U};
    ws2812_effect_double_flash(&invalid, 0, 0);
    ws2812_effect_alternating_halves(&invalid, 0, 0);
    frame.led_count = WS2812_STRIP0_LED_COUNT;

    faults.levels.p2 = 0xFFU;
    faults.levels.p3 = 0x1FFFU;
    tick();
    assert_sent_color(0U); /* P2/P3 alone do not take over lighting. */
    assert(!ws2812_app_is_active());
    faults.raw = 0U;
    faults.bits.water_detected = 1U;
    for (unsigned ms = 0; ms < 1000; ms += 50) {
        tick();
        ws2812_effect_alternating_halves(&frame, 0xFF0000U, ms);
        assert(memcmp(sent_colors, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
    }
    faults.bits.water_detected = 0U;
    faults.bits.water_offline = 1U;
    for (unsigned i = 0; i < 60; i++) {
        tick();
        assert(ws2812_app_is_active());
        assert(sent_colors[0] == 0xFF0000U || sent_colors[frame.led_count - 1] == 0xFF0000U);
    }
    faults.bits.water_offline = 0U;
    faults.bits.water_sensor_fault = 1U;
    for (unsigned i = 0; i < 60; i++) { tick(); assert(ws2812_app_is_active()); }

    faults.bits.platform_limit_conflict = 1U; /* Immediate upgrade, phase starts at zero. */
    for (unsigned ms = 0; ms < 1500; ms += 50) {
        tick();
        assert_sent_color(ms < 150 || (ms >= 300 && ms < 450) ? 0xFF0000U : 0U);
        assert(ws2812_app_is_active());
    }
    faults.bits.door_limit_conflict = 1U;
    faults.bits.emergency_stop = 1U;
    assert(debug_console_apply_command("WSE:3"));
    ws2812_app_set_door_state(E_DOOR_RUN_IN_ACTION);
    for (unsigned i = 0; i < 35; i++) { tick(); assert_sent_color(0xFF0000U); }
    faults.bits.emergency_stop = 0U;
    for (unsigned i = 0; i < 20; i++) { tick(); assert_sent_color(0xFF0000U); }
    faults.bits.emergency_stop = 1U; /* Recurrence cancels the pending downgrade. */
    tick();
    faults.bits.emergency_stop = 0U;
    for (unsigned i = 0; i < 40; i++) { tick(); assert_sent_color(0xFF0000U); }
    tick(); /* 2000 ms after first absent sample: switch to limit double flash. */
    for (unsigned i = 0; i < 3; i++) tick();
    assert_sent_color(0U);

    faults.bits.door_limit_conflict = faults.bits.platform_limit_conflict = 0U;
    for (unsigned i = 0; i < 41; i++) tick();
    assert(sent_colors[0] == 0xFF0000U && sent_colors[frame.led_count - 1] == 0U); /* Held water survived the override. */
    faults.bits.water_sensor_fault = 0U; /* Valid dry reading permits recovery. */
    for (unsigned i = 0; i < 40; i++) { tick(); assert(ws2812_app_is_active()); }
    tick();
    expected_debug(2, 0U); /* Debug selection was saved, not rendered during P1. */
    assert(memcmp(sent_colors, work_colors, frame.led_count * sizeof(uint32_t)) == 0);
    assert(debug_console_apply_command("WSE:0"));
    tick();
    assert(ws2812_app_is_active()); /* Current business input resumes. */
    ws2812_app_set_door_state(E_DOOR_RUN_IDLE);
    tick();
    assert_sent_color(0U);
    assert(!ws2812_app_is_active());
}

int main(int argc, char **argv)
{
    assert(WS2812_APP_PERIOD_MS == 50);
    check_basic_effects();
    check_new_effects();
    check_frame_ownership();
    check_commands();
    check_alarm_effects();
    /* Optional preview exports actual C-rendered frames, not a second animation implementation. */
    if (argc == 2 && strcmp(argv[1], "--preview") == 0) {
        puts("[");
        for (unsigned effect = 0; effect < 6; effect++) {
            puts(effect ? ",[" : "[");
            for (unsigned elapsed = 0; elapsed < 24000; elapsed += 50) {
                effects[effect](&frame, elapsed);
                printf(elapsed ? ",[" : "[");
                for (unsigned i = 0; i < frame.led_count; i++) printf(i ? ",%u" : "%u", (unsigned)work_colors[i]);
                putchar(']');
            }
            puts("]");
        }
        puts("]");
    }
    return 0;
}
