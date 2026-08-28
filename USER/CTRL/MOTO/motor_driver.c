#include "board.h"

#include "motor_driver.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_timer.h"

/*
 * 推杆电机 PWM 驱动（新硬件拓扑）
 *
 * 每个推杆电机由「推出 / 收回」两路 PWM 控制（同一 TIMER 的两个通道），
 * 驱动板高电平有效。未驱动方向输出低电平并关闭通道。
 *
 *   平台: TIMER1 CH0=PA0(推出) CH1=PA1(收回)
 *   门1 : TIMER3 CH0=PB6(推出) CH1=PB7(收回)
 *   门2 : TIMER3 CH2=PB8(推出) CH3=PB9(收回)
 *
 * 真值表（extend/retract 为是否驱动该方向）：
 *   extend=0 retract=0  → 停止（低 / 低）
 *   extend=1 retract=0  → 推出（高 / 低）
 *   extend=0 retract=1  → 收回（低 / 高）
 *   extend=1 retract=1  → 制动（高 / 高）
 *
 * 到位/限位信号读低触发（上拉输入，读到低 = 到位）：
 *   平台到位: 推出PA4 收回PA5；平台限位: 推出PB12 收回PB13
 *   门1到位 : 推出PC8 收回PC9；门2到位 : 推出PC6 收回PC7
 *   门限位  : 开到位PB15 关到位PB14（门组公共）
 */

/** 驱动方向的固定高电平占空比。 */
#ifndef MODULE_MOTO_PWM_HIGH_DUTY
#define MODULE_MOTO_PWM_HIGH_DUTY 99U
#endif

typedef struct {
    board_pwm_id_e pwm;                 /* 所属 TIMER（board_pwm_configs 索引） */
    board_pwm_ch_id_e extend_ch;        /* 推出通道 id（ch id） */
    board_pwm_ch_id_e retract_ch;       /* 收回通道 id */
    board_pin_id_e extend_fb_pin;       /* 推出到位信号（读低） */
    board_pin_id_e retract_fb_pin;      /* 收回到位信号（读低） */
} module_moto_hw_config_t;

typedef struct {
    const module_moto_hw_config_t *hw;
    module_moto_dir_e direction;
    bool extend_output_active;
    bool retract_output_active;
} module_moto_context_t;

static const module_moto_hw_config_t motor_hw_configs[MODULE_MOTO_ID_COUNT] = {
    [PLATFORM_MOTOR_ID] = {
        BOARD_PWM_PLATFORM, BOARD_PWM_CH1, BOARD_PWM_CH0,
        BOARD_PIN_PLATFORM_EXTEND_SIGNAL, BOARD_PIN_PLATFORM_RETRACT_SIGNAL,
    },
    [DOOR1_MOTOR_ID] = {
        BOARD_PWM_DOOR, BOARD_PWM_CH1, BOARD_PWM_CH0,
        BOARD_PIN_DOOR1_EXTEND_SIGNAL, BOARD_PIN_DOOR1_RETRACT_SIGNAL,
    },
    [DOOR2_MOTOR_ID] = {
        BOARD_PWM_DOOR, BOARD_PWM_CH3, BOARD_PWM_CH2,
        BOARD_PIN_DOOR2_EXTEND_SIGNAL, BOARD_PIN_DOOR2_RETRACT_SIGNAL,
    },
};

/* 由 (TIMER, ch id) 解析出电机某方向的 (通道实例, io) 绑定 */
static const board_pwm_channel_config_t *module_moto_ch(
    const module_moto_hw_config_t *hw, bool extend)
{
    board_pwm_ch_id_e ch_id = extend ? hw->extend_ch : hw->retract_ch;

    return &board_pwm_configs[hw->pwm].ch[ch_id];
}

static module_moto_context_t motor_contexts[MODULE_MOTO_ID_COUNT];
static bool motor_initialized;

static bool module_moto_id_is_valid(module_moto_id_e motor_id)
{
    return ((int)motor_id >= 0) && (motor_id < MODULE_MOTO_ID_COUNT);
}

static bool module_moto_is_ready(module_moto_id_e motor_id)
{
    if (!module_moto_id_is_valid(motor_id)) {
        return false;
    }

    return motor_initialized;
}

/* 读低触发：读到低电平 = 到位 */
static uint8_t module_moto_read_feedback(board_pin_id_e pin_id)
{
    FlagStatus level = gpio_input_bit_get(BOARD_PIN_PORT(pin_id), BOARD_PIN_PIN(pin_id));

    return ((level == SET) ? 1U : 0U) == (MODULE_MOTO_FB_ACTIVE_LEVEL) ? 1U : 0U;
}

/* 三台推杆的到位反馈，均为读低有效。 */
#define HELI_EXTEND_STOP_SIGNAL  (module_moto_read_feedback(BOARD_PIN_PLATFORM_EXTEND_SIGNAL) != 0U)
#define HELI_RETRACT_STOP_SIGNAL (module_moto_read_feedback(BOARD_PIN_PLATFORM_RETRACT_SIGNAL) != 0U)
#define DOOR1_EXTEND_STOP_SIGNAL (module_moto_read_feedback(BOARD_PIN_DOOR1_EXTEND_SIGNAL) != 0U)
#define DOOR1_RETRACT_STOP_SIGNAL (module_moto_read_feedback(BOARD_PIN_DOOR1_RETRACT_SIGNAL) != 0U)
#define DOOR2_EXTEND_STOP_SIGNAL (module_moto_read_feedback(BOARD_PIN_DOOR2_EXTEND_SIGNAL) != 0U)
#define DOOR2_RETRACT_STOP_SIGNAL (module_moto_read_feedback(BOARD_PIN_DOOR2_RETRACT_SIGNAL) != 0U)

static bool module_moto_direction_is_blocked(
    module_moto_id_e motor_id,
    module_moto_dir_e direction)
{
    if (!module_moto_id_is_valid(motor_id)) {
        return true;
    }

    if (direction == MODULE_MOTO_DIR_FORWARD) {
        return module_moto_is_at_forward(motor_id);
    }

    if (direction == MODULE_MOTO_DIR_BACKWARD) {
        return module_moto_is_at_backward(motor_id);
    }

    return false;
}

static uint32_t module_moto_duty_to_pulse(const board_pwm_config_t *pwm,
                                          uint8_t duty_percent)
{
    uint32_t period = pwm->period + 1U;

    if (duty_percent >= 100U) {
        return period;
    }

    return (period * duty_percent) / 100U;
}

static void module_moto_set_channel_output(board_pwm_id_e pwm_id,
                                           const board_pwm_channel_config_t *ch,
                                           bool active)
{
    const board_pwm_config_t *pwm = &board_pwm_configs[pwm_id];
    uint8_t duty_percent = active ? MODULE_MOTO_PWM_HIGH_DUTY : 0U;
    uint32_t pulse = module_moto_duty_to_pulse(pwm, duty_percent);

    timer_channel_output_pulse_value_config(pwm->peripheral, ch->channel,
                                            (uint16_t)pulse);
    timer_channel_output_state_config(pwm->peripheral, ch->channel,
                                      active ? TIMER_CCX_ENABLE : TIMER_CCX_DISABLE);
}

/* extend/retract 为是否驱动（true=该方向输出高电平）。 */
static void module_moto_drive(
    module_moto_context_t *motor_context,
    bool extend_active,
    bool retract_active)
{
    const module_moto_hw_config_t *hw;

    if (motor_context == 0) {
        return;
    }

    hw = motor_context->hw;
    if (!extend_active) {
        module_moto_set_channel_output(hw->pwm, module_moto_ch(hw, true), false);
    }
    if (!retract_active) {
        module_moto_set_channel_output(hw->pwm, module_moto_ch(hw, false), false);
    }
    if (extend_active) {
        module_moto_set_channel_output(hw->pwm, module_moto_ch(hw, true), true);
    }
    if (retract_active) {
        module_moto_set_channel_output(hw->pwm, module_moto_ch(hw, false), true);
    }

    motor_context->extend_output_active = extend_active;
    motor_context->retract_output_active = retract_active;
}

static void module_moto_gpio_clock_enable(uint32_t port)
{
    switch (port) {
    case GPIOA: rcu_periph_clock_enable(RCU_GPIOA); break;
    case GPIOB: rcu_periph_clock_enable(RCU_GPIOB); break;
    case GPIOC: rcu_periph_clock_enable(RCU_GPIOC); break;
    case GPIOD: rcu_periph_clock_enable(RCU_GPIOD); break;
    default: break;
    }
}

static void module_moto_pwm_channel_init(board_pwm_id_e pwm_id,
                                         const board_pwm_channel_config_t *ch)
{
    const board_pwm_config_t *pwm = &board_pwm_configs[pwm_id];
    timer_oc_parameter_struct ocpara;

    module_moto_gpio_clock_enable(BOARD_PIN_PORT(ch->pin_id));
    gpio_init(BOARD_PIN_PORT(ch->pin_id), board_gpio_configs[ch->pin_id].mode,
              board_gpio_configs[ch->pin_id].speed, BOARD_PIN_PIN(ch->pin_id));

    timer_channel_output_struct_para_init(&ocpara);
    ocpara.outputstate = TIMER_CCX_DISABLE;
    ocpara.ocpolarity = TIMER_OC_POLARITY_HIGH;
    ocpara.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    timer_channel_output_config(pwm->peripheral, ch->channel, &ocpara);
    timer_channel_output_mode_config(pwm->peripheral, ch->channel, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(pwm->peripheral, ch->channel,
                                       TIMER_OC_SHADOW_DISABLE);
    module_moto_set_channel_output(pwm_id, ch, false);
}

static void module_moto_pwm_peripheral_init(board_pwm_id_e pwm_id)
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

bool module_moto_init(void)
{
    bool pwm_inited[BOARD_PWM_COUNT] = { false, false };

    if (motor_initialized) {
        return true;
    }

    /* JTAG 关闭（保留 SWD）：释放 PA15/PB3/PB4 等被 JTAG 占用的引脚 */
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    for (int motor_index = 0; motor_index < MODULE_MOTO_ID_COUNT; motor_index++) {
        module_moto_context_t *motor_context = &motor_contexts[motor_index];
        const module_moto_hw_config_t *hw = &motor_hw_configs[motor_index];
        board_pwm_id_e pwm_id = hw->pwm;

        motor_context->hw = hw;

        if (!pwm_inited[pwm_id]) {
            module_moto_pwm_peripheral_init(pwm_id);
            pwm_inited[pwm_id] = true;
        }

        module_moto_pwm_channel_init(pwm_id, module_moto_ch(hw, true));
        module_moto_pwm_channel_init(pwm_id, module_moto_ch(hw, false));

        /* 到位信号：上拉输入，读低触发 */
        module_moto_gpio_clock_enable(BOARD_PIN_PORT(hw->extend_fb_pin));
        gpio_init(BOARD_PIN_PORT(hw->extend_fb_pin),
                  board_gpio_configs[hw->extend_fb_pin].mode,
                  board_gpio_configs[hw->extend_fb_pin].speed,
                  BOARD_PIN_PIN(hw->extend_fb_pin));
        gpio_init(BOARD_PIN_PORT(hw->retract_fb_pin),
                  board_gpio_configs[hw->retract_fb_pin].mode,
                  board_gpio_configs[hw->retract_fb_pin].speed,
                  BOARD_PIN_PIN(hw->retract_fb_pin));

        motor_context->direction = MODULE_MOTO_DIR_STOP;
        module_moto_drive(motor_context, false, false);
    }

    /* 平台、门组公共限位：上拉输入，读低触发 */
    module_moto_gpio_clock_enable(BOARD_PIN_PORT(BOARD_PIN_PLATFORM_EXTEND_LIMIT));
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_PLATFORM_EXTEND_LIMIT),
              board_gpio_configs[BOARD_PIN_PLATFORM_EXTEND_LIMIT].mode,
              board_gpio_configs[BOARD_PIN_PLATFORM_EXTEND_LIMIT].speed,
              BOARD_PIN_PIN(BOARD_PIN_PLATFORM_EXTEND_LIMIT));
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_PLATFORM_RETRACT_LIMIT),
              board_gpio_configs[BOARD_PIN_PLATFORM_RETRACT_LIMIT].mode,
              board_gpio_configs[BOARD_PIN_PLATFORM_RETRACT_LIMIT].speed,
              BOARD_PIN_PIN(BOARD_PIN_PLATFORM_RETRACT_LIMIT));

    module_moto_gpio_clock_enable(BOARD_PIN_PORT(BOARD_PIN_DOOR_OPEN_LIMIT));
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_DOOR_OPEN_LIMIT),
              board_gpio_configs[BOARD_PIN_DOOR_OPEN_LIMIT].mode,
              board_gpio_configs[BOARD_PIN_DOOR_OPEN_LIMIT].speed,
              BOARD_PIN_PIN(BOARD_PIN_DOOR_OPEN_LIMIT));
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_DOOR_CLOSE_LIMIT),
              board_gpio_configs[BOARD_PIN_DOOR_CLOSE_LIMIT].mode,
              board_gpio_configs[BOARD_PIN_DOOR_CLOSE_LIMIT].speed,
              BOARD_PIN_PIN(BOARD_PIN_DOOR_CLOSE_LIMIT));

    /* 电吸锁低有效，初始化为释放状态，避免上电时误锁。 */
    gpio_init(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK),
              board_gpio_configs[BOARD_PIN_DOOR_LOCK].mode,
              board_gpio_configs[BOARD_PIN_DOOR_LOCK].speed,
              BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK));
    gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK),
                   BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK), SET);

    /* 启动各 PWM 外设 */
    for (int pwm_index = 0; pwm_index < BOARD_PWM_COUNT; pwm_index++) {
        if (pwm_inited[pwm_index]) {
            timer_primary_output_config(board_pwm_configs[pwm_index].peripheral,
                                        ENABLE);
            timer_enable(board_pwm_configs[pwm_index].peripheral);
        }
    }

    motor_initialized = true;
    return true;
}

bool module_moto_stop(module_moto_id_e motor_id)
{
    module_moto_context_t *motor_context;

    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];
    module_moto_drive(motor_context, false, false);
    motor_context->direction = MODULE_MOTO_DIR_STOP;

    return true;
}

bool module_moto_brake(module_moto_id_e motor_id)
{
    module_moto_context_t *motor_context;

    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];
    module_moto_drive(motor_context, true, true);
    motor_context->direction = MODULE_MOTO_DIR_BRAKE;

    return true;
}

void module_moto_stop_all(void)
{
    for (int motor_index = 0; motor_index < MODULE_MOTO_ID_COUNT; motor_index++) {
        module_moto_context_t *motor_context = &motor_contexts[motor_index];

        if (motor_context->hw != NULL) {
            module_moto_drive(motor_context, false, false);
        }

        motor_context->direction = MODULE_MOTO_DIR_STOP;
    }
}

bool module_moto_start(module_moto_id_e motor_id, module_moto_dir_e direction)
{
    module_moto_context_t *motor_context;

    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];

    switch (direction) {
    case MODULE_MOTO_DIR_FORWARD:
        if (module_moto_direction_is_blocked(motor_id, direction)) {
            module_moto_drive(motor_context, false, false);
            motor_context->direction = MODULE_MOTO_DIR_STOP;
            return false;
        }
        module_moto_drive(motor_context, true, false);
        motor_context->direction = MODULE_MOTO_DIR_FORWARD;
        return true;

    case MODULE_MOTO_DIR_BACKWARD:
        if (module_moto_direction_is_blocked(motor_id, direction)) {
            module_moto_drive(motor_context, false, false);
            motor_context->direction = MODULE_MOTO_DIR_STOP;
            return false;
        }
        module_moto_drive(motor_context, false, true);
        motor_context->direction = MODULE_MOTO_DIR_BACKWARD;
        return true;

    default:
        break;
    }

    return false;
}

bool module_moto_is_at_forward(module_moto_id_e motor_id)
{
    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    switch (motor_id) {
    case PLATFORM_MOTOR_ID:
        return HELI_EXTEND_STOP_SIGNAL;

    case DOOR1_MOTOR_ID:
        return DOOR1_EXTEND_STOP_SIGNAL;

    case DOOR2_MOTOR_ID:
        return DOOR2_EXTEND_STOP_SIGNAL;

    default:
        return false;
    }
}

bool module_moto_is_at_backward(module_moto_id_e motor_id)
{
    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    switch (motor_id) {
    case PLATFORM_MOTOR_ID:
        return HELI_RETRACT_STOP_SIGNAL;

    case DOOR1_MOTOR_ID:
        return DOOR1_RETRACT_STOP_SIGNAL;

    case DOOR2_MOTOR_ID:
        return DOOR2_RETRACT_STOP_SIGNAL;

    default:
        return false;
    }
}

module_moto_pos_e module_moto_get_position(module_moto_id_e motor_id)
{
    bool at_forward;
    bool at_backward;

    if (!module_moto_is_ready(motor_id)) {
        return MODULE_MOTO_POS_ERROR;
    }

    at_forward = module_moto_is_at_forward(motor_id);
    at_backward = module_moto_is_at_backward(motor_id);

    if (at_forward && at_backward) {
        return MODULE_MOTO_POS_ERROR;
    }

    if (at_forward) {
        return MODULE_MOTO_POS_FORWARD;
    }

    if (at_backward) {
        return MODULE_MOTO_POS_BACKWARD;
    }

    return MODULE_MOTO_POS_NONE;
}

module_moto_dir_e module_moto_get_direction(module_moto_id_e motor_id)
{
    if (!module_moto_is_ready(motor_id)) {
        return MODULE_MOTO_DIR_STOP;
    }

    return motor_contexts[motor_id].direction;
}

void module_moto_get_driver_status(module_moto_id_e motor_id,
                                   module_moto_driver_status_t *status)
{
    const module_moto_context_t *motor_context;

    if (status == 0) {
        return;
    }

    status->extend_output_active = 0U;
    status->retract_output_active = 0U;
    status->extend_feedback_active = 0U;
    status->retract_feedback_active = 0U;

    if (!module_moto_is_ready(motor_id)) {
        return;
    }

    motor_context = &motor_contexts[motor_id];
    status->extend_output_active = motor_context->extend_output_active ? 1U : 0U;
    status->retract_output_active = motor_context->retract_output_active ? 1U : 0U;
    status->extend_feedback_active = module_moto_is_at_forward(motor_id) ? 1U : 0U;
    status->retract_feedback_active = module_moto_is_at_backward(motor_id) ? 1U : 0U;
}

void module_moto_door_lock(void)
{
    if (!motor_initialized) {
        return;
    }

    gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK),
                   BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK), RESET);
}

void module_moto_door_unlock(void)
{
    if (!motor_initialized) {
        return;
    }

    gpio_bit_write(BOARD_PIN_PORT(BOARD_PIN_DOOR_LOCK),
                   BOARD_PIN_PIN(BOARD_PIN_DOOR_LOCK), SET);
}

bool module_moto_platform_extend_limit_active(void)
{
    return (module_moto_read_feedback(BOARD_PIN_PLATFORM_EXTEND_LIMIT) != 0U);
}

bool module_moto_platform_retract_limit_active(void)
{
    return (module_moto_read_feedback(BOARD_PIN_PLATFORM_RETRACT_LIMIT) != 0U);
}

bool module_moto_door_open_limit_active(void)
{
    return (module_moto_read_feedback(BOARD_PIN_DOOR_OPEN_LIMIT) != 0U);
}

bool module_moto_door_close_limit_active(void)
{
    return (module_moto_read_feedback(BOARD_PIN_DOOR_CLOSE_LIMIT) != 0U);
}
