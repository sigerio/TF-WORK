#include "light_output.h"

#include "board.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"

/* 保留全局符号，便于调试器 Watch 直接查看三路独立灯光输出状态。 */
light_output_status_t light_status;

static bool light_output_initialized;

static void light_output_write(board_pin_id_e pin_id, bool on)
{
    gpio_bit_write(BOARD_PIN_PORT(pin_id), BOARD_PIN_PIN(pin_id), on ? RESET : SET);
}

void light_output_init(void)
{
    const board_pin_id_e light_pins[] = {
        BOARD_PIN_LED_INDICATOR,
        BOARD_PIN_LED_INTERIOR,
        BOARD_PIN_LED_STRIP_ENABLE,
    };

    rcu_periph_clock_enable(RCU_GPIOC);

    for (uint8_t pin_index = 0U;
         pin_index < (uint8_t)(sizeof(light_pins) / sizeof(light_pins[0]));
         pin_index++) {
        board_pin_id_e pin_id = light_pins[pin_index];

        gpio_init(BOARD_PIN_PORT(pin_id), board_gpio_configs[pin_id].mode,
                  board_gpio_configs[pin_id].speed, BOARD_PIN_PIN(pin_id));
        light_output_write(pin_id, false);
    }

    light_status.indicator_on = 0U;
    light_status.interior_light_on = 0U;
    light_status.decorative_light_on = 0U;
    light_output_initialized = true;
}

void light_output_set_indicator(bool on)
{
    if (!light_output_initialized) {
        return;
    }

    light_output_write(BOARD_PIN_LED_INDICATOR, on);
    light_status.indicator_on = on ? 1U : 0U;
}

void light_output_set_interior_light(bool on)
{
    if (!light_output_initialized) {
        return;
    }

    light_output_write(BOARD_PIN_LED_INTERIOR, on);
    light_status.interior_light_on = on ? 1U : 0U;
}

void light_output_set_decorative_light(bool on)
{
    if (!light_output_initialized) {
        return;
    }

    light_output_write(BOARD_PIN_LED_STRIP_ENABLE, on);
    light_status.decorative_light_on = on ? 1U : 0U;
}

void light_output_set_indicator_on(void)
{
    light_output_set_indicator(true);
}

void light_output_set_indicator_off(void)
{
    light_output_set_indicator(false);
}

void light_output_set_interior_light_on(void)
{
    light_output_set_interior_light(true);
}

void light_output_set_interior_light_off(void)
{
    light_output_set_interior_light(false);
}

void light_output_set_decorative_light_on(void)
{
    light_output_set_decorative_light(true);
}

void light_output_set_decorative_light_off(void)
{
    light_output_set_decorative_light(false);
}

void light_output_get_status(light_output_status_t *status)
{
    if (status == 0) {
        return;
    }

    *status = light_status;
}
