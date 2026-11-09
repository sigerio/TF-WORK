#include "CONTROL/output/aux_output.h"

#include "BOARD/board.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"

/* 保留全局符号，便于调试器 Watch 直接查看指示灯、内部照明和伴热带使能状态。 */
aux_output_status_t output_status;

static bool aux_output_initialized;

static void aux_output_write(board_pin_id_e pin_id, bool enabled)
{
    gpio_bit_write(BOARD_PIN_PORT(pin_id), BOARD_PIN_PIN(pin_id), enabled ? RESET : SET);
}

void aux_output_init(void)
{
    const board_pin_id_e output_pins[] = {
        BOARD_PIN_LED_INDICATOR,
        BOARD_PIN_LED_INTERIOR,
        BOARD_PIN_DOOR_HEATER_ENABLE,
    };

    rcu_periph_clock_enable(RCU_GPIOC);

    for (uint8_t pin_index = 0U; pin_index < (uint8_t)(sizeof(output_pins) / sizeof(output_pins[0])); pin_index++) {
        board_pin_id_e pin_id = output_pins[pin_index];

        gpio_init(BOARD_PIN_PORT(pin_id), board_gpio_configs[pin_id].mode, board_gpio_configs[pin_id].speed, BOARD_PIN_PIN(pin_id));
        aux_output_write(pin_id, false);
    }

    output_status.indicator_enabled = 0U;
    output_status.interior_light_enabled = 0U;
    output_status.door_heater_enabled = 0U;
    aux_output_initialized = true;
}

void aux_output_enable_indicator(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_LED_INDICATOR, true);
    output_status.indicator_enabled = 1U;
}

void aux_output_disable_indicator(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_LED_INDICATOR, false);
    output_status.indicator_enabled = 0U;
}

void aux_output_enable_interior_light(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_LED_INTERIOR, true);
    output_status.interior_light_enabled = 1U;
}

void aux_output_disable_interior_light(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_LED_INTERIOR, false);
    output_status.interior_light_enabled = 0U;
}

void aux_output_enable_door_heater(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_DOOR_HEATER_ENABLE, true);
    output_status.door_heater_enabled = 1U;
}

void aux_output_disable_door_heater(void)
{
    if (!aux_output_initialized) {
        return;
    }

    aux_output_write(BOARD_PIN_DOOR_HEATER_ENABLE, false);
    output_status.door_heater_enabled = 0U;
}

void aux_output_get_status(aux_output_status_t *status)
{
    if (status == 0) {
        return;
    }

    *status = output_status;
}
