#include "BOARD/board.h"

#include "CONTROL/motor/motor_driver.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_timer.h"

/* 高有效双通道：正向 CH1/CH1/CH3，反向 CH0/CH0/CH2；关闭旧方向后才打开新方向。 */

/** 驱动方向的固定高电平占空比。 */
#ifndef MOTOR_PWM_HIGH_DUTY
#define MOTOR_PWM_HIGH_DUTY 99U
#endif

typedef struct {
    board_pwm_id_e pwm;                 /* 所属 TIMER（board_pwm_configs 索引） */
    board_pwm_ch_id_e forward_ch;        /* 正向通道 */
    board_pwm_ch_id_e backward_ch;       /* 反向通道 */
} motor_driver_hw_config_t;

typedef struct {
    const motor_driver_hw_config_t *hw;
    motor_direction_e direction;
} motor_driver_context_t;

static const motor_driver_hw_config_t motor_hw_configs[MOTOR_ID_COUNT] = {
    [MOTOR_PLATFORM] = { BOARD_PWM_PLATFORM, BOARD_PWM_CH1, BOARD_PWM_CH0 },
    [MOTOR_DOOR1] = { BOARD_PWM_DOOR, BOARD_PWM_CH1, BOARD_PWM_CH0 },
    [MOTOR_DOOR2] = { BOARD_PWM_DOOR, BOARD_PWM_CH3, BOARD_PWM_CH2 },
};

/* 由 (TIMER, ch id) 解析出电机某方向的 (通道实例, io) 绑定 */
static const board_pwm_channel_config_t *motor_driver_ch(const motor_driver_hw_config_t *hw, bool forward)
{
    board_pwm_ch_id_e ch_id = forward ? hw->forward_ch : hw->backward_ch;

    return &board_pwm_configs[hw->pwm].ch[ch_id];
}

static motor_driver_context_t motor_contexts[MOTOR_ID_COUNT];
static bool motor_initialized;

static bool motor_driver_is_ready(motor_id_e motor_id)
{
    return motor_initialized && ((int)motor_id >= 0) && (motor_id < MOTOR_ID_COUNT);
}

static uint32_t motor_driver_duty_to_pulse(const board_pwm_config_t *pwm, uint8_t duty_percent)
{
    uint32_t period = pwm->period + 1U;

    if (duty_percent >= 100U) {
        return period;
    }

    return (period * duty_percent) / 100U;
}

static void motor_driver_set_channel_output(board_pwm_id_e pwm_id, const board_pwm_channel_config_t *ch, bool active)
{
    const board_pwm_config_t *pwm = &board_pwm_configs[pwm_id];
    uint8_t duty_percent = active ? MOTOR_PWM_HIGH_DUTY : 0U;
    uint32_t pulse = motor_driver_duty_to_pulse(pwm, duty_percent);

    timer_channel_output_pulse_value_config(pwm->peripheral, ch->channel, (uint16_t)pulse);
    timer_channel_output_state_config(pwm->peripheral, ch->channel, active ? TIMER_CCX_ENABLE : TIMER_CCX_DISABLE);
}

/* forward/backward 为是否驱动（true=该方向输出高电平）。 */
static void motor_driver_drive(motor_driver_context_t *motor_context, bool forward_active, bool backward_active)
{
    const motor_driver_hw_config_t *hw;

    hw = motor_context->hw;
    if (!forward_active) {
        motor_driver_set_channel_output(hw->pwm, motor_driver_ch(hw, true), false);
    }
    if (!backward_active) {
        motor_driver_set_channel_output(hw->pwm, motor_driver_ch(hw, false), false);
    }
    if (forward_active) {
        motor_driver_set_channel_output(hw->pwm, motor_driver_ch(hw, true), true);
    }
    if (backward_active) {
        motor_driver_set_channel_output(hw->pwm, motor_driver_ch(hw, false), true);
    }
}

static void motor_driver_gpio_clock_enable(uint32_t port)
{
    switch (port) {
    case GPIOA: rcu_periph_clock_enable(RCU_GPIOA); break;
    case GPIOB: rcu_periph_clock_enable(RCU_GPIOB); break;
    case GPIOC: rcu_periph_clock_enable(RCU_GPIOC); break;
    case GPIOD: rcu_periph_clock_enable(RCU_GPIOD); break;
    default: break;
    }
}

static void motor_driver_pwm_channel_init(board_pwm_id_e pwm_id, const board_pwm_channel_config_t *ch)
{
    const board_pwm_config_t *pwm = &board_pwm_configs[pwm_id];
    timer_oc_parameter_struct ocpara;

    motor_driver_gpio_clock_enable(BOARD_PIN_PORT(ch->pin_id));
    gpio_init(BOARD_PIN_PORT(ch->pin_id), board_gpio_configs[ch->pin_id].mode,
        board_gpio_configs[ch->pin_id].speed, BOARD_PIN_PIN(ch->pin_id));

    timer_channel_output_struct_para_init(&ocpara);
    ocpara.outputstate = TIMER_CCX_DISABLE;
    ocpara.ocpolarity = TIMER_OC_POLARITY_HIGH;
    ocpara.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    timer_channel_output_config(pwm->peripheral, ch->channel, &ocpara);
    timer_channel_output_mode_config(pwm->peripheral, ch->channel, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(pwm->peripheral, ch->channel, TIMER_OC_SHADOW_DISABLE);
    motor_driver_set_channel_output(pwm_id, ch, false);
}

static void motor_driver_pwm_peripheral_init(board_pwm_id_e pwm_id)
{
    const board_pwm_config_t *pwm = &board_pwm_configs[pwm_id];
    timer_parameter_struct timer_para;

    rcu_periph_clock_enable(pwm->clock);
    timer_deinit(pwm->peripheral);
    timer_struct_para_init(&timer_para);
    timer_para.prescaler = (uint16_t)pwm->prescaler;
    timer_para.alignedmode = TIMER_COUNTER_EDGE;
    timer_para.counterdirection = TIMER_COUNTER_UP;
    timer_para.period = pwm->period;
    timer_para.clockdivision = TIMER_CKDIV_DIV1;
    timer_para.repetitioncounter = 0U;
    timer_init(pwm->peripheral, &timer_para);
    timer_auto_reload_shadow_enable(pwm->peripheral);
}

bool motor_driver_init(void)
{
    bool pwm_inited[BOARD_PWM_COUNT] = { false, false };

    if (motor_initialized) {
        return true;
    }

    /* JTAG 关闭（保留 SWD）：释放 PA15/PB3/PB4 等被 JTAG 占用的引脚 */
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    for (int motor_index = 0; motor_index < MOTOR_ID_COUNT; motor_index++) {
        motor_driver_context_t *motor_context = &motor_contexts[motor_index];
        const motor_driver_hw_config_t *hw = &motor_hw_configs[motor_index];
        board_pwm_id_e pwm_id = hw->pwm;

        motor_context->hw = hw;

        if (!pwm_inited[pwm_id]) {
            motor_driver_pwm_peripheral_init(pwm_id);
            pwm_inited[pwm_id] = true;
        }

        motor_driver_pwm_channel_init(pwm_id, motor_driver_ch(hw, true));
        motor_driver_pwm_channel_init(pwm_id, motor_driver_ch(hw, false));

        motor_context->direction = MOTOR_DIR_STOP;
        motor_driver_drive(motor_context, false, false);
    }

    /* 启动各 PWM 外设 */
    for (int pwm_index = 0; pwm_index < BOARD_PWM_COUNT; pwm_index++) {
        if (pwm_inited[pwm_index]) {
            timer_primary_output_config(board_pwm_configs[pwm_index].peripheral, ENABLE);
            timer_enable(board_pwm_configs[pwm_index].peripheral);
        }
    }

    motor_initialized = true;
    return true;
}

bool motor_driver_stop(motor_id_e motor_id)
{
    motor_driver_context_t *motor_context;

    if (!motor_driver_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];
    motor_driver_drive(motor_context, false, false);
    motor_context->direction = MOTOR_DIR_STOP;

    return true;
}

bool motor_driver_brake(motor_id_e motor_id)
{
    motor_driver_context_t *motor_context;

    if (!motor_driver_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];
    motor_driver_drive(motor_context, true, true);
    motor_context->direction = MOTOR_DIR_BRAKE;

    return true;
}

bool motor_driver_start(motor_id_e motor_id, motor_direction_e direction)
{
    motor_driver_context_t *motor_context;

    if (!motor_driver_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];

    switch (direction) {
    case MOTOR_DIR_FORWARD:
        motor_driver_drive(motor_context, true, false);
        motor_context->direction = MOTOR_DIR_FORWARD;
        return true;

    case MOTOR_DIR_BACKWARD:
        motor_driver_drive(motor_context, false, true);
        motor_context->direction = MOTOR_DIR_BACKWARD;
        return true;

    default:
        break;
    }

    return false;
}

motor_direction_e motor_driver_get_direction(motor_id_e motor_id)
{
    if (!motor_driver_is_ready(motor_id)) {
        return MOTOR_DIR_STOP;
    }

    return motor_contexts[motor_id].direction;
}
