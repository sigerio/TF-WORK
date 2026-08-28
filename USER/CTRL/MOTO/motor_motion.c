#include "motor_motion.h"

#include "../../../USER/OS/tf_os.h"
#include "../../LIB/m_bit.h"
#include "../../TASK/tf_usr_event_task.h"
#include "../module_ws2812/module_ws2812_status.h"
#include "motor_driver.h"


/**
 * 运动阶段：
 * - MOTION_START : 已收到请求，等待启动
 * - MOTION_ING   : 运动进行中，等待到位
 * - MOTION_OVER  : 空闲或本次动作结束
 * - MOTION_ERR   : 启动失败，记录错误并转入结束
 */
typedef enum
{
    MOTION_START = 0,
    MOTION_ING,
    MOTION_OVER,
    MOTION_ERR
} motion_phase_e;

/** 门组 / 平台运动错误 */
typedef enum
{
    MOTION_ERROR_NONE = 0,
    MOTION_ERROR_DOOR_START_FAILED,
    MOTION_ERROR_PLATFORM_START_FAILED,
} motion_error_e;

/** 单路运动上下文（门组 / 平台各一份） */
typedef struct
{
    motion_phase_e phase;                 /* 当前阶段 */
    module_moto_dir_e direction;          /* 目标运行方向 */
    module_moto_pos_e target_position;    /* 目标到位位置（电机反馈） */
    motion_error_e motion_error;          /* 本次动作保留的错误信息 */
    uint8_t start_err_cnt;                /* 本次尝试内连续启动失败次数 */
} motion_context_t;


/* 双门运动上下文，默认空闲 */
static motion_context_t door_motion = {
    .phase = MOTION_OVER,
    .direction = MODULE_MOTO_DIR_STOP,
    .target_position = MODULE_MOTO_POS_NONE,
    .motion_error = MOTION_ERROR_NONE,
    .start_err_cnt = 0U,
};

/* 平台单电机运动上下文，默认空闲 */
static motion_context_t platform_motion = {
    .phase = MOTION_OVER,
    .direction = MODULE_MOTO_DIR_STOP,
    .target_position = MODULE_MOTO_POS_NONE,
    .motion_error = MOTION_ERROR_NONE,
    .start_err_cnt = 0U,
};


/* 保留全局符号，便于调试器 Watch 直接查看门 / 平台业务状态。 */
module_moto_run_status_t run_status;

static void refresh_motor_debug_status(void)
{
    module_moto_get_driver_status(PLATFORM_MOTOR_ID, &run_status.platform_motor);
    module_moto_get_driver_status(DOOR1_MOTOR_ID, &run_status.door1_motor);
    module_moto_get_driver_status(DOOR2_MOTOR_ID, &run_status.door2_motor);
    run_status.platform_extend_limit_active =
        module_moto_platform_extend_limit_active() ? 1U : 0U;
    run_status.platform_retract_limit_active =
        module_moto_platform_retract_limit_active() ? 1U : 0U;
    run_status.door_open_limit_active = module_moto_door_open_limit_active() ? 1U : 0U;
    run_status.door_close_limit_active = module_moto_door_close_limit_active() ? 1U : 0U;
}

static void trigger_door_state_event(door_running_in_e running)
{
    (void)tf_os_event_trigger(TF_USR_EVENT_DOOR_STATE,
                              &running,
                              sizeof(running),
                              1U);
}

static void trigger_platform_state_event(platform_running_in_e running)
{
    (void)tf_os_event_trigger(TF_USR_EVENT_PLATFORM_STATE,
                              &running,
                              sizeof(running),
                              1U);
}

static void set_door_motion_phase(motion_phase_e phase)
{
    if ((door_motion.phase != MOTION_OVER) && (phase == MOTION_OVER))
    {
        trigger_door_state_event(E_DOOR_RUN_IDLE);
    }

    door_motion.phase = phase;
    if (phase == MOTION_OVER)
    {
        cancel_signal(OS_DOOR_S);
    }
    else
    {
        set_signal(OS_DOOR_S);
    }
}

static void set_platform_motion_phase(motion_phase_e phase)
{
    if ((platform_motion.phase != MOTION_OVER) && (phase == MOTION_OVER))
    {
        trigger_platform_state_event(E_PLATFORM_RUN_IDLE);
    }

    platform_motion.phase = phase;
    if (phase == MOTION_OVER)
    {
        cancel_signal(OS_HELIPAD_S);
    }
    else
    {
        set_signal(OS_HELIPAD_S);
    }
}

/** 同时停止两扇门电机 */
static void stop_door_motors(void)
{
    (void)module_moto_stop(DOOR1_MOTOR_ID);
    (void)module_moto_stop(DOOR2_MOTOR_ID);
}

/** 门是否到达目标位置（双门对应方向的到位反馈均有效）。 */
static bool door_at_target(module_moto_pos_e target)
{
    if (target == MODULE_MOTO_POS_FORWARD)
    {
        return module_moto_is_at_forward(DOOR1_MOTOR_ID) &&
               module_moto_is_at_forward(DOOR2_MOTOR_ID);
    }

    if (target == MODULE_MOTO_POS_BACKWARD)
    {
        return module_moto_is_at_backward(DOOR1_MOTOR_ID) &&
               module_moto_is_at_backward(DOOR2_MOTOR_ID);
    }

    return false;
}

static bool door_feedback_is_invalid(void)
{
    return (module_moto_is_at_forward(DOOR1_MOTOR_ID) &&
            module_moto_is_at_backward(DOOR1_MOTOR_ID)) ||
           (module_moto_is_at_forward(DOOR2_MOTOR_ID) &&
            module_moto_is_at_backward(DOOR2_MOTOR_ID));
}

/** 根据双门到位反馈刷新门组业务状态。 */
static void refresh_door_run_status(void)
{
    if (door_feedback_is_invalid())
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
    }
    else if (door_at_target(MODULE_MOTO_POS_BACKWARD))
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_OPENED;
    }
    else if (door_at_target(MODULE_MOTO_POS_FORWARD))
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_CLOSED;
    }
    else
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_INTERMEDIATE;
    }
    stop_door_motors();
}

static void lock_door_if_closed(void)
{
    if (run_status.door == MODULE_MOTO_DOOR_STATE_CLOSED)
    {
        module_moto_door_lock();
    }
}

/** 根据平台当前到位反馈刷新业务状态。 */
static void refresh_platform_run_status(void)
{
    module_moto_pos_e platform_position;

    platform_position = module_moto_get_position(PLATFORM_MOTOR_ID);

    if (platform_position == MODULE_MOTO_POS_ERROR)
    {
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_FAULT;
    }
    else if (platform_position == MODULE_MOTO_POS_FORWARD)
    {
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_EXTENDED;
    }
    else if (platform_position == MODULE_MOTO_POS_BACKWARD)
    {
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_RETRACTED;
    }
    else
    {
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_INTERMEDIATE;
    }
}


/**
 * @brief  启动双门运动（MOTION_START 阶段处理）。
 * @return 下一阶段：已到目标反馈则 OVER；启动成功则 ING；
 *         启动失败则 ERR
 * @note   门开关到位以两扇门的对应方向反馈均有效为准。
 */
static motion_phase_e start_door_motion(void)
{
    if (door_feedback_is_invalid())
    {
        stop_door_motors();
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
        return MOTION_ERR;
    }

    /* 已在目标反馈：无需动作 */
    if (door_at_target(door_motion.target_position))
    {
        stop_door_motors();
        refresh_door_run_status();
        lock_door_if_closed();
        return MOTION_OVER;
    }


    /* 双门紧挨启动，尽量缩短启动间隔；失败统一停机 */
    {
        bool door1_started;
        bool door2_started;

        door1_started = module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);
        door2_started = module_moto_start(DOOR2_MOTOR_ID, door_motion.direction);

        if (!door1_started || !door2_started)
        {
            door_motion.start_err_cnt++;
            stop_door_motors();
        }
        else
        {
            door_motion.start_err_cnt = 0U;
            run_status.door = MODULE_MOTO_DOOR_STATE_INTERMEDIATE;
            return MOTION_ING;
        }

        if (door_motion.start_err_cnt >= 3U)
        {
            door_motion.start_err_cnt = 0U;
            door_motion.motion_error = MOTION_ERROR_DOOR_START_FAILED;
            return MOTION_ERR;
        }
    }

    return MOTION_START;
}

/**
 * @brief  双门运行期更新（MOTION_ING 阶段处理）。
 * @return 下一阶段：双门均到达目标反馈则 OVER，否则保持 ING
 *
 * 默认策略：
 * 1. 双门均到达目标到位反馈 → 停机 OVER
 * 2. 单个电机到位信号触发 → 单独停止该电机
 *
 * 开启门同步宏后，额外按霍尔边沿差调节双门，并检测堵转。
 */
static motion_phase_e update_door_motion(void)
{

    if (door_feedback_is_invalid())
    {
        stop_door_motors();
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
        return MOTION_ERR;
    }

    /* 双门均到达目标到位反馈 */
    if (door_at_target(door_motion.target_position))
    {
        stop_door_motors();
        refresh_door_run_status();
        lock_door_if_closed();
        return MOTION_OVER;
    }

    /* 单个电机到位信号触发：单独停止该电机（其方向已被限位阻断） */
    if (module_moto_is_at_forward(DOOR1_MOTOR_ID) &&
        (door_motion.direction == MODULE_MOTO_DIR_FORWARD))
    {
        (void)module_moto_stop(DOOR1_MOTOR_ID);
    }
    if (module_moto_is_at_backward(DOOR1_MOTOR_ID) &&
        (door_motion.direction == MODULE_MOTO_DIR_BACKWARD))
    {
        (void)module_moto_stop(DOOR1_MOTOR_ID);
    }
    if (module_moto_is_at_forward(DOOR2_MOTOR_ID) &&
        (door_motion.direction == MODULE_MOTO_DIR_FORWARD))
    {
        (void)module_moto_stop(DOOR2_MOTOR_ID);
    }
    if (module_moto_is_at_backward(DOOR2_MOTOR_ID) &&
        (door_motion.direction == MODULE_MOTO_DIR_BACKWARD))
    {
        (void)module_moto_stop(DOOR2_MOTOR_ID);
    }


    return MOTION_ING;
}

/**
 * @brief  启动平台运动（MOTION_START 阶段处理）。
 * @return 下一阶段：已到位 OVER；启动成功 ING；启动失败 ERR
 */
static motion_phase_e start_platform_motion(void)
{
    module_moto_pos_e platform_position;

    platform_position = module_moto_get_position(PLATFORM_MOTOR_ID);

    if (platform_position == MODULE_MOTO_POS_ERROR)
    {
        (void)module_moto_stop(PLATFORM_MOTOR_ID);
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_FAULT;
        return MOTION_ERR;
    }

    if (platform_position == platform_motion.target_position)
    {
        (void)module_moto_stop(PLATFORM_MOTOR_ID);
        refresh_platform_run_status();
        return MOTION_OVER;
    }

    if (!module_moto_start(PLATFORM_MOTOR_ID, platform_motion.direction))
    {
        (void)module_moto_stop(PLATFORM_MOTOR_ID);
        platform_motion.start_err_cnt++;
    }
    else
    {
        platform_motion.start_err_cnt = 0U;
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_INTERMEDIATE;
        return MOTION_ING;
    }

    if (platform_motion.start_err_cnt >= 3U)
    {
        platform_motion.start_err_cnt = 0U;
        platform_motion.motion_error = MOTION_ERROR_PLATFORM_START_FAILED;
        return MOTION_ERR;
    }

    return MOTION_START;
}

/**
 * @brief  平台运行期更新（MOTION_ING 阶段处理）。
 * @return 到位则停机 OVER，否则保持 ING
 * @note   当前无堵转/总超时保护，依赖限位反馈结束。
 */
static motion_phase_e update_platform_motion(void)
{
    module_moto_pos_e platform_position;

    platform_position = module_moto_get_position(PLATFORM_MOTOR_ID);

    if (platform_position == MODULE_MOTO_POS_ERROR)
    {
        (void)module_moto_stop(PLATFORM_MOTOR_ID);
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_FAULT;
        return MOTION_ERR;
    }

    if (platform_position != platform_motion.target_position)
    {
        return MOTION_ING;
    }

    (void)module_moto_stop(PLATFORM_MOTOR_ID);
    refresh_platform_run_status();

    return MOTION_OVER;
}

/**
 * @brief  门故障处理（MOTION_ERR 阶段）。
 * @note   START_FAILED / BLOCKED：双门应为 STOP。
 */
static motion_phase_e handle_door_motion_error(void)
{
    switch (door_motion.motion_error)
    {
    case MOTION_ERROR_DOOR_START_FAILED:
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;

        if ((module_moto_get_direction(DOOR1_MOTOR_ID) != MODULE_MOTO_DIR_STOP) ||
            (module_moto_get_direction(DOOR2_MOTOR_ID) != MODULE_MOTO_DIR_STOP))
        {
            stop_door_motors();
        }
        return MOTION_OVER;

    default:
        return MOTION_OVER;
    }
}

/**
 * @brief  平台故障处理（MOTION_ERR 阶段）。
 * @note   启动失败时平台电机应为 STOP。
 */
static motion_phase_e handle_platform_motion_error(void)
{
    switch (platform_motion.motion_error)
    {
    case MOTION_ERROR_PLATFORM_START_FAILED:
        run_status.platform = MODULE_MOTO_PLATFORM_STATE_FAULT;

        if (module_moto_get_direction(PLATFORM_MOTOR_ID) != MODULE_MOTO_DIR_STOP)
        {
            (void)module_moto_stop(PLATFORM_MOTOR_ID);
        }
        return MOTION_OVER;

    default:
        return MOTION_OVER;
    }
}

void module_moto_sync_init(void)
{
    cancel_signal(OS_DOOR_S);
    cancel_signal(OS_HELIPAD_S);
    (void)module_moto_brake(DOOR1_MOTOR_ID);
    (void)module_moto_brake(DOOR2_MOTOR_ID);
    refresh_door_run_status();
    lock_door_if_closed();
    refresh_platform_run_status();
    refresh_motor_debug_status();
}

void module_moto_sync_get_run_status(module_moto_run_status_t *status)
{
    if (status == 0)
    {
        return;
    }

    *status = run_status;
}

void module_moto_sync_stop_door(void)
{
    stop_door_motors();
    set_door_motion_phase(MOTION_OVER);
    door_motion.direction = MODULE_MOTO_DIR_STOP;
    door_motion.target_position = MODULE_MOTO_POS_NONE;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
    refresh_door_run_status();
    lock_door_if_closed();
    refresh_motor_debug_status();
}

void module_moto_sync_stop_platform(void)
{
    (void)module_moto_stop(PLATFORM_MOTOR_ID);
    set_platform_motion_phase(MOTION_OVER);
    platform_motion.direction = MODULE_MOTO_DIR_STOP;
    platform_motion.target_position = MODULE_MOTO_POS_NONE;
    platform_motion.motion_error = MOTION_ERROR_NONE;
    platform_motion.start_err_cnt = 0U;
    refresh_platform_run_status();
    refresh_motor_debug_status();
}

void module_moto_sync_request_door_open(void)
{
    module_moto_door_unlock();
    set_door_motion_phase(MOTION_START);
    door_motion.direction = MODULE_MOTO_DIR_BACKWARD;
    door_motion.target_position = MODULE_MOTO_POS_BACKWARD;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
    trigger_door_state_event(E_DOOR_RUN_IN_ACTION);
}

void module_moto_sync_request_door_close(void)
{
    set_door_motion_phase(MOTION_START);
    door_motion.direction = MODULE_MOTO_DIR_FORWARD;
    door_motion.target_position = MODULE_MOTO_POS_FORWARD;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
    trigger_door_state_event(E_DOOR_RUN_IN_ACTION);
}

void module_moto_sync_request_platform_extend(void)
{
    set_platform_motion_phase(MOTION_START);
    platform_motion.direction = MODULE_MOTO_DIR_FORWARD;
    platform_motion.target_position = MODULE_MOTO_POS_FORWARD;
    platform_motion.motion_error = MOTION_ERROR_NONE;
    platform_motion.start_err_cnt = 0U;
    trigger_platform_state_event(E_PLATFORM_RUN_IN_ACTION);
}

void module_moto_sync_request_platform_retract(void)
{
    set_platform_motion_phase(MOTION_START);
    platform_motion.direction = MODULE_MOTO_DIR_BACKWARD;
    platform_motion.target_position = MODULE_MOTO_POS_BACKWARD;
    platform_motion.motion_error = MOTION_ERROR_NONE;
    platform_motion.start_err_cnt = 0U;
    trigger_platform_state_event(E_PLATFORM_RUN_IN_ACTION);
}

void module_moto_sync_update(void)
{
    /* 双门状态机 */
    switch (door_motion.phase)
    {
    case MOTION_START:
        set_door_motion_phase(start_door_motion());
        break;

    case MOTION_ING:
        set_door_motion_phase(update_door_motion());
        break;

    case MOTION_OVER:
        break;

    case MOTION_ERR:
        set_door_motion_phase(handle_door_motion_error());
        break;
    }

    /* 平台状态机（与门独立并行） */
    switch (platform_motion.phase)
    {
    case MOTION_START:
        set_platform_motion_phase(start_platform_motion());
        break;

    case MOTION_ING:
        set_platform_motion_phase(update_platform_motion());
        break;

    case MOTION_OVER:
        break;

    case MOTION_ERR:
        set_platform_motion_phase(handle_platform_motion_error());
        break;
    }

    refresh_motor_debug_status();
}
