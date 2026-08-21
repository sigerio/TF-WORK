#include "board.h"

#include "key.h"

#include <string.h>

#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"

#define TF_KEY_ACTIVE_LEVEL_PLATFORM 0U
#define TF_KEY_ACTIVE_LEVEL_DOOR 0U
#define TF_KEY_ACTIVE_LEVEL_EM_STOP 0U


enum {
    KEY_DEFAULT_CYCLE_MS = 50U
};

typedef struct {
    board_pin_id_e pin;
    uint8_t active_level;
} tf_key_hw_config_t;

tf_key_info_t g_tf_key_info = {0};

static uint16_t key_pressed_bitmap;
static bool key_scan_ready;
static const tf_key_hw_config_t key_hardware_configs[TF_KEY_ID_COUNT] = {
    { BOARD_PIN_KEY_PLATFORM, TF_KEY_ACTIVE_LEVEL_PLATFORM },
    { BOARD_PIN_KEY_DOOR, TF_KEY_ACTIVE_LEVEL_DOOR },
    { BOARD_PIN_KEY_EMERGENCY_STOP, TF_KEY_ACTIVE_LEVEL_EM_STOP },
};

static bool tf_key_id_is_valid(tf_key_id_t key)
{
    return ((int)key >= 0) && (key < TF_KEY_ID_COUNT);
}

static uint8_t tf_key_read_pressed(tf_key_id_t key)
{
    if (!tf_key_id_is_valid(key)) {
        return 0U;
    }

    const board_gpio_config_t *pin = &board_gpio_configs[key_hardware_configs[key].pin];
    uint8_t level = (gpio_input_bit_get(pin->port, pin->pin) == SET) ? 1U : 0U;
    return (level == key_hardware_configs[key].active_level) ? 1U : 0U;
}

static void tf_key_update_pressed_bitmap(void)
{
    uint16_t value = 0U;

    for (int i = 0; i < TF_KEY_ID_COUNT; i++) {
        if (i == TF_KEY_ID_EM_STOP) {
            continue;
        }

        value |= (uint16_t)(tf_key_read_pressed((tf_key_id_t)i) << i);
    }

    key_pressed_bitmap = value;
}

static uint16_t tf_key_get_pressed_bitmap(void)
{
    tf_key_update_pressed_bitmap();
    return key_pressed_bitmap;
}

static uint16_t tf_key_elapsed_add(uint16_t elapsed_ms, uint32_t cycle_ms)
{
    if (cycle_ms == 0U) {
        cycle_ms = KEY_DEFAULT_CYCLE_MS;
    }

    uint32_t next_ms = (uint32_t)elapsed_ms + cycle_ms;
    if (next_ms > TF_KEY_MAX_MS) {
        next_ms = TF_KEY_MAX_MS;
    }

    return (uint16_t)next_ms;
}

static void tf_key_scan_state(uint16_t now_key, uint32_t cycle_ms)
{
    tf_key_info_t *key_info = &g_tf_key_info;
    static uint16_t last_key_bitmap = 0x0000U;

    for (int i = 0; i < TF_KEY_ID_COUNT; i++) {
        if (i == TF_KEY_ID_EM_STOP) {
            continue;
        }

        uint16_t old_pressed = (uint16_t)((last_key_bitmap >> i) & 0x01U);
        uint16_t new_pressed = (uint16_t)((now_key >> i) & 0x01U);

        if (old_pressed == new_pressed) {
            if (new_pressed != 0U) {
                uint16_t prev_time = key_info->key_time[i];

                key_info->key_time[i] = tf_key_elapsed_add(key_info->key_time[i], cycle_ms);

                if (key_info->key_time[i] >= TF_KEY_MAX_MS) {
                    key_info->key_event[i] = TF_KEY_EVENT_LONG_REPEAT;
                } else if ((prev_time < TF_KEY_LONG_MS) &&
                           (key_info->key_time[i] >= TF_KEY_LONG_MS)) {
                    key_info->key_event[i] = TF_KEY_EVENT_LONG;
                    key_info->key_state[i] = old_pressed;
                }
            } else {
                key_info->key_time[i] = 0U;
            }
        } else {
            if (new_pressed != 0U) {
                key_info->key_time[i] = tf_key_elapsed_add(key_info->key_time[i], cycle_ms);
            } else {
                if ((key_info->key_time[i] > TF_KEY_SHORT_MIN_MS) &&
                    (key_info->key_time[i] < TF_KEY_SHORT_MAX_MS)) {
                    key_info->key_event[i] = TF_KEY_EVENT_SHORT;
                    key_info->key_state[i] = old_pressed;
                } else if (key_info->key_time[i] < TF_KEY_SHORT_MIN_MS) {
                    key_info->key_event[i] = TF_KEY_EVENT_RESET;
                }
                key_info->key_time[i] = 0U;
            }
        }
    }

    last_key_bitmap = now_key;
}

bool tf_key_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    for (int key_index = 0; key_index < TF_KEY_ID_COUNT; key_index++) {
        const board_gpio_config_t *pin =
            &board_gpio_configs[key_hardware_configs[key_index].pin];

        gpio_init(pin->port, pin->mode, pin->speed, pin->pin);
    }

    tf_key_reset_state();
    return true;
}

void tf_key_reset_state(void)
{
    memset(&g_tf_key_info, 0, sizeof(g_tf_key_info));
    key_pressed_bitmap = 0U;
    key_scan_ready = false;
}


void tf_key_scan_task(uint32_t cycle_ms)
{
    uint16_t now_key;

    now_key = tf_key_get_pressed_bitmap();
    if ((now_key == 0U) && (!key_scan_ready)) {
        key_scan_ready = true;
    }

    if (!key_scan_ready) {
        return;
    }

    tf_key_scan_state(now_key, cycle_ms);
}


tf_key_info_t tf_key_get_info(void)
{
    return g_tf_key_info;
}

bool tf_key_read_event(tf_key_id_t key, tf_key_event_t event)
{
    tf_key_info_t info = tf_key_get_info();

    if (!tf_key_id_is_valid(key)) {
        return false;
    }

    return (info.key_event[key] == event);
}

void tf_key_clear_event(void)
{
    for (int i = 0; i < TF_KEY_ID_COUNT; i++) {
        if (i == TF_KEY_ID_EM_STOP) {
            continue;
        }

        g_tf_key_info.key_state[i] = 0U;
        g_tf_key_info.key_event[i] = TF_KEY_EVENT_RESET;
    }
}

bool tf_key_is_active(tf_key_id_t key)
{
    tf_key_info_t info = tf_key_get_info();

    if (!tf_key_id_is_valid(key)) {
        return false;
    }

    return (info.key_time[key] != 0U);
}

bool tf_key_is_pressed_now(tf_key_id_t key)
{
    if (!tf_key_id_is_valid(key)) {
        return false;
    }

    if (key == TF_KEY_ID_EM_STOP) {
        return (tf_key_read_pressed(key) != 0U);
    }

    return (((key_pressed_bitmap >> key) & 0x01U) != 0U);
}
