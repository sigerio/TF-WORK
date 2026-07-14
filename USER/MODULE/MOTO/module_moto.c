#include "module_moto.h"

#include "tf_drv_io.h"

/*
电机驱动真值表

IN1  IN2  res
L    L      stop
H    L      forward
L    H      reverse
H    H      brake

*/

typedef struct {
    int ctrl1_io_id;
    int ctrl2_io_id;
    int forward_fb_io_id;
    int backward_fb_io_id;
} module_moto_hw_config_t;

typedef struct {
    tf_drv_io_handle_t ctrl1;
    tf_drv_io_handle_t ctrl2;
    tf_drv_io_handle_t forward_fb;
    tf_drv_io_handle_t backward_fb;
    module_moto_dir_e direction;
} module_moto_context_t;

static const module_moto_hw_config_t motor_hw_configs[MODULE_MOTO_ID_COUNT] = {
    [HELIPED_MOTOR_ID] = {
        E_IO_HELIPED_CTRL1,
        E_IO_HELIPED_CTRL2,
        E_IO_HELIPED_FORW_FB,
        E_IO_HELIPED_BACK_FB,
    },
    [DOOR0_MOTOR_ID] = {
        E_IO_DOOR0_CTRL1,
        E_IO_DOOR0_CTRL2,
        E_IO_DOOR0_FORW_FB,
        E_IO_DOOR0_BACK_FB,
    },
    [DOOR1_MOTOR_ID] = {
        E_IO_DOOR1_CTRL1,
        E_IO_DOOR1_CTRL2,
        E_IO_DOOR1_FORW_FB,
        E_IO_DOOR1_BACK_FB,
    },
};

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

static uint8_t module_moto_read_feedback(tf_drv_io_handle_t handle)
{
    if (handle == 0) {
        return 0U;
    }

    return (tf_drv_io_get(handle) == MODULE_MOTO_FB_ACTIVE_LEVEL) ? 1U : 0U;
}

static bool module_moto_direction_is_blocked(
    const module_moto_context_t *motor_context,
    module_moto_dir_e direction)
{
    if (motor_context == 0) {
        return true;
    }

    if (direction == MODULE_MOTO_DIR_FORWARD) {
        return (module_moto_read_feedback(motor_context->forward_fb) != 0U);
    }

    if (direction == MODULE_MOTO_DIR_BACKWARD) {
        return (module_moto_read_feedback(motor_context->backward_fb) != 0U);
    }

    return false;
}

static void module_moto_drive(
    module_moto_context_t *motor_context,
    uint8_t ctrl1_level,
    uint8_t ctrl2_level)
{
    if (motor_context == 0) {
        return;
    }

    tf_drv_io_set(motor_context->ctrl1, ctrl1_level);
    tf_drv_io_set(motor_context->ctrl2, ctrl2_level);
}

bool module_moto_init(void)
{
    bool init_success = true;

    if (motor_initialized) {
        return true;
    }

    for (int motor_index = 0; motor_index < MODULE_MOTO_ID_COUNT; motor_index++) {
        module_moto_context_t *motor_context = &motor_contexts[motor_index];
        const module_moto_hw_config_t *hardware_config = &motor_hw_configs[motor_index];

        motor_context->ctrl1 = tf_drv_io_register(hardware_config->ctrl1_io_id);
        motor_context->ctrl2 = tf_drv_io_register(hardware_config->ctrl2_io_id);
        motor_context->forward_fb = tf_drv_io_register(hardware_config->forward_fb_io_id);
        motor_context->backward_fb = tf_drv_io_register(hardware_config->backward_fb_io_id);
        motor_context->direction = MODULE_MOTO_DIR_STOP;

        if ((motor_context->ctrl1 == 0) || (motor_context->ctrl2 == 0) ||
            (motor_context->forward_fb == 0) || (motor_context->backward_fb == 0)) {
            init_success = false;
        }

        if ((motor_context->ctrl1 != 0) && (motor_context->ctrl2 != 0)) {
            module_moto_drive(motor_context, 0U, 0U);
        }
    }

    motor_initialized = init_success;
    if (!init_success) {
        module_moto_stop_all();
    }

    return init_success;
}

bool module_moto_stop(module_moto_id_e motor_id)
{
    module_moto_context_t *motor_context;

    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    motor_context = &motor_contexts[motor_id];
    module_moto_drive(motor_context, 0U, 0U);
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
    module_moto_drive(motor_context, 1U, 1U);
    motor_context->direction = MODULE_MOTO_DIR_BRAKE;

    return true;
}

void module_moto_stop_all(void)
{
    for (int motor_index = 0; motor_index < MODULE_MOTO_ID_COUNT; motor_index++) {
        module_moto_context_t *motor_context = &motor_contexts[motor_index];

        if ((motor_context->ctrl1 != 0) && (motor_context->ctrl2 != 0)) {
            module_moto_drive(motor_context, 0U, 0U);
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
        if (module_moto_direction_is_blocked(motor_context, direction)) {
            module_moto_drive(motor_context, 0U, 0U);
            motor_context->direction = MODULE_MOTO_DIR_STOP;
            return false;
        }
        module_moto_drive(motor_context, 1U, 0U);
        motor_context->direction = MODULE_MOTO_DIR_FORWARD;
        return true;

    case MODULE_MOTO_DIR_BACKWARD:
        if (module_moto_direction_is_blocked(motor_context, direction)) {
            module_moto_drive(motor_context, 0U, 0U);
            motor_context->direction = MODULE_MOTO_DIR_STOP;
            return false;
        }
        module_moto_drive(motor_context, 0U, 1U);
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

    return (module_moto_read_feedback(motor_contexts[motor_id].forward_fb) != 0U);
}

bool module_moto_is_at_backward(module_moto_id_e motor_id)
{
    if (!module_moto_is_ready(motor_id)) {
        return false;
    }

    return (module_moto_read_feedback(motor_contexts[motor_id].backward_fb) != 0U);
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
