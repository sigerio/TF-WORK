#include "module_moto_hall.h"

#if MODULE_MOTO_HALL_ENABLE

#include "tf_drv_io.h"

#include "gd32f10x_exti.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_rcu.h"

#ifndef MODULE_MOTO_HALL_IRQ_PREEMPT_PRIORITY
#define MODULE_MOTO_HALL_IRQ_PREEMPT_PRIORITY 1U
#endif

#ifndef MODULE_MOTO_HALL_IRQ_SUB_PRIORITY
#define MODULE_MOTO_HALL_IRQ_SUB_PRIORITY 1U
#endif

typedef struct {
    int io_id;
    uint8_t gpio_port_source;
    uint8_t gpio_pin_source;
    exti_line_enum exti_line;
} module_moto_hall_hw_config_t;

static const module_moto_hall_hw_config_t hall_hw_configs[MODULE_MOTO_HALL_ID_COUNT] = {
    [MODULE_MOTO_HALL_ID_DOOR0] = {
        E_IO_DOOR0_HALL,
        GPIO_PORT_SOURCE_GPIOA,
        GPIO_PIN_SOURCE_15,
        EXTI_15,
    },
    [MODULE_MOTO_HALL_ID_DOOR1] = {
        E_IO_DOOR1_HALL,
        GPIO_PORT_SOURCE_GPIOC,
        GPIO_PIN_SOURCE_11,
        EXTI_11,
    },
};

static tf_drv_io_handle_t hall_handles[MODULE_MOTO_HALL_ID_COUNT];
static volatile uint32_t hall_edge_counts[MODULE_MOTO_HALL_ID_COUNT];
static bool hall_initialized;

static bool module_moto_hall_id_is_valid(module_moto_hall_id_e hall_id)
{
    return ((int)hall_id >= 0) && (hall_id < MODULE_MOTO_HALL_ID_COUNT);
}

static void module_moto_hall_count_edge(module_moto_hall_id_e hall_id)
{
    if (hall_initialized && module_moto_hall_id_is_valid(hall_id)) {
        hall_edge_counts[hall_id]++;
    }
}

bool module_moto_hall_init(void)
{
    bool init_success = true;

    if (hall_initialized) {
        return true;
    }

    /*
     * PA15 defaults to JTAG JTDI. Disable JTAG while retaining SWD so
     * E_IO_DOOR0_HALL can be initialized as a regular GPIO/EXTI input.
     */
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    for (int hall_index = 0; hall_index < MODULE_MOTO_HALL_ID_COUNT; hall_index++) {
        hall_handles[hall_index] = tf_drv_io_register(hall_hw_configs[hall_index].io_id);
        if (hall_handles[hall_index] == NULL) {
            init_success = false;
        }
    }

    if (!init_success) {
        return false;
    }

    for (int hall_index = 0; hall_index < MODULE_MOTO_HALL_ID_COUNT; hall_index++) {
        const module_moto_hall_hw_config_t *hardware_config = &hall_hw_configs[hall_index];

        gpio_exti_source_select(hardware_config->gpio_port_source, hardware_config->gpio_pin_source);
        exti_init(hardware_config->exti_line, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
        exti_flag_clear(hardware_config->exti_line);
        hall_edge_counts[hall_index] = 0U;
    }

    hall_initialized = true;
    nvic_irq_enable(EXTI10_15_IRQn,
                    MODULE_MOTO_HALL_IRQ_PREEMPT_PRIORITY,
                    MODULE_MOTO_HALL_IRQ_SUB_PRIORITY);

    return true;
}

bool module_moto_hall_is_initialized(void)
{
    return hall_initialized;
}

uint8_t module_moto_hall_get_level(module_moto_hall_id_e hall_id)
{
    if (!module_moto_hall_id_is_valid(hall_id) || (hall_handles[hall_id] == NULL)) {
        return 0U;
    }

    return tf_drv_io_get(hall_handles[hall_id]);
}

uint32_t module_moto_hall_get_edge_count(module_moto_hall_id_e hall_id)
{
    if (!module_moto_hall_id_is_valid(hall_id)) {
        return 0U;
    }

    return hall_edge_counts[hall_id];
}

void module_moto_hall_reset_edge_count(module_moto_hall_id_e hall_id)
{
    uint32_t previous_primask;

    if (!module_moto_hall_id_is_valid(hall_id)) {
        return;
    }

    previous_primask = __get_PRIMASK();
    __disable_irq();
    hall_edge_counts[hall_id] = 0U;
    if (previous_primask == 0U) {
        __enable_irq();
    }
}

void module_moto_hall_reset_all_edge_counts(void)
{
    uint32_t previous_primask = __get_PRIMASK();

    __disable_irq();
    for (int hall_index = 0; hall_index < MODULE_MOTO_HALL_ID_COUNT; hall_index++) {
        hall_edge_counts[hall_index] = 0U;
    }
    if (previous_primask == 0U) {
        __enable_irq();
    }
}

void module_moto_hall_exti10_15_irq_handler(void)
{
    if (exti_flag_get(EXTI_15) != RESET) {
        exti_flag_clear(EXTI_15);
        module_moto_hall_count_edge(MODULE_MOTO_HALL_ID_DOOR0);
    }

    if (exti_flag_get(EXTI_11) != RESET) {
        exti_flag_clear(EXTI_11);
        module_moto_hall_count_edge(MODULE_MOTO_HALL_ID_DOOR1);
    }
}

#endif
