#include "module_moto_sync.h"

#include "module_moto.h"
#include "module_moto_hall.h"

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
/* 双门霍尔边沿差 >= 该值：停领先门，只跑落后门 */
#define DOOR_SYNC_STOP_EDGE_DIFF 4U
/* 双门霍尔边沿差 < 该值：双门同时运行（与 STOP 形成滞后带，避免频繁启停） */
#define DOOR_SYNC_RESUME_EDGE_DIFF 2U
/* 门电机运动中持续无霍尔边沿超过该时间，判为堵转并停机 */
#define DOOR_MOTOR_BLOCK_TIMEOUT_MS 200U
#endif

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

/** 门组 / 停机坪运动错误 */
typedef enum
{
    MOTION_ERROR_NONE = 0,
    MOTION_ERROR_DOOR_START_FAILED,
#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
    MOTION_ERROR_DOOR_BLOCKED,
#endif
    MOTION_ERROR_HELIPED_START_FAILED,
} motion_error_e;

/** 单路运动上下文（门组 / 停机坪各一份） */
typedef struct
{
    motion_phase_e phase;                 /* 当前阶段 */
    module_moto_dir_e direction;          /* 目标运行方向 */
    module_moto_pos_e target_position;    /* 目标到位位置（限位反馈） */
    motion_error_e motion_error;          /* 本次动作保留的错误信息 */
    uint8_t start_err_cnt;                /* 本次尝试内连续启动失败次数 */
} motion_context_t;

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
/** 单门堵转监视：运动中若霍尔边沿长时间不变则超时 */
typedef struct
{
    uint32_t last_edge_count;  /* 上次采样的霍尔边沿计数 */
    uint32_t no_edge_time_ms;  /* 边沿未变化累计时间（ms） */
} door_motor_block_monitor_t;
#endif

/* 双门运动上下文，默认空闲 */
static motion_context_t door_motion = {
    .phase = MOTION_OVER,
    .direction = MODULE_MOTO_DIR_STOP,
    .target_position = MODULE_MOTO_POS_NONE,
    .motion_error = MOTION_ERROR_NONE,
    .start_err_cnt = 0U,
};

/* 停机坪单电机运动上下文，默认空闲 */
static motion_context_t heliped_motion = {
    .phase = MOTION_OVER,
    .direction = MODULE_MOTO_DIR_STOP,
    .target_position = MODULE_MOTO_POS_NONE,
    .motion_error = MOTION_ERROR_NONE,
    .start_err_cnt = 0U,
};

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
/* 双门各自堵转监视器，在每次门动作启动时复位 */
static door_motor_block_monitor_t door0_block_monitor;
static door_motor_block_monitor_t door1_block_monitor;
#endif

/* 门 / 停机坪业务运行状态，由 sync 初始化和运动流程维护。 */
static module_moto_run_status_t run_status;

/** 同时停止两扇门电机 */
static void stop_door_motors(void)
{
    (void)module_moto_stop(DOOR0_MOTOR_ID);
    (void)module_moto_stop(DOOR1_MOTOR_ID);
}

/** 根据双门当前限位刷新门组业务状态。 */
static void refresh_door_run_status(void)
{
    module_moto_pos_e door0_position;
    module_moto_pos_e door1_position;

    door0_position = module_moto_get_position(DOOR0_MOTOR_ID);
    door1_position = module_moto_get_position(DOOR1_MOTOR_ID);

    if ((door0_position == MODULE_MOTO_POS_ERROR) ||
        (door1_position == MODULE_MOTO_POS_ERROR))
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
    }
    else if ((door0_position == MODULE_MOTO_POS_FORWARD) &&
             (door1_position == MODULE_MOTO_POS_FORWARD))
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_OPENED;
    }
    else if ((door0_position == MODULE_MOTO_POS_BACKWARD) &&
             (door1_position == MODULE_MOTO_POS_BACKWARD))
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_CLOSED;
    }
    else
    {
        run_status.door = MODULE_MOTO_DOOR_STATE_INTERMEDIATE;
    }
}

/** 根据停机坪当前限位刷新业务状态。 */
static void refresh_heliped_run_status(void)
{
    module_moto_pos_e heliped_position;

    heliped_position = module_moto_get_position(HELIPED_MOTOR_ID);

    if (heliped_position == MODULE_MOTO_POS_ERROR)
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_FAULT;
    }
    else if (heliped_position == MODULE_MOTO_POS_FORWARD)
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_EXTENDED;
    }
    else if (heliped_position == MODULE_MOTO_POS_BACKWARD)
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_RETRACTED;
    }
    else
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_INTERMEDIATE;
    }
}

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
/**
 * @brief  更新单门堵转计时，判断是否超时。
 * @param  motor_id       电机 ID
 * @param  edge_count     当前霍尔边沿计数
 * @param  block_monitor  该门的堵转监视器
 * @return true  已堵转超时；false 正常或电机未在正/反转
 * @note   仅在 direction 为 FORWARD/BACKWARD 时累计；
 *         边沿有变化则清零计时。
 */
static bool door_motor_block_timed_out(
    module_moto_id_e motor_id,
    uint32_t edge_count,
    door_motor_block_monitor_t *block_monitor)
{
    module_moto_dir_e direction;

    direction = module_moto_get_direction(motor_id);

    /* 未在驱动方向上运行：清零监视，不判堵转 */
    if ((direction != MODULE_MOTO_DIR_FORWARD) &&
        (direction != MODULE_MOTO_DIR_BACKWARD))
    {
        block_monitor->last_edge_count = edge_count;
        block_monitor->no_edge_time_ms = 0U;
        return false;
    }

    /* 霍尔仍有边沿：说明在动，清零超时计时 */
    if (edge_count != block_monitor->last_edge_count)
    {
        block_monitor->last_edge_count = edge_count;
        block_monitor->no_edge_time_ms = 0U;
        return false;
    }

    /* 边沿不变：按调度周期累加无边沿时间 */
    block_monitor->no_edge_time_ms += MODULE_MOTO_SYNC_MONITOR_PERIOD_MS;

    return block_monitor->no_edge_time_ms >= DOOR_MOTOR_BLOCK_TIMEOUT_MS;
}
#endif

/**
 * @brief  启动双门运动（MOTION_START 阶段处理）。
 * @return 下一阶段：双门均已到位则 OVER；启动成功则 ING；
 *         任意单门已在目标位或启动失败则 ERR
 * @note   开启门同步宏后，启动前复位霍尔边沿计数与堵转监视。
 *         任意门已在目标位时不启动电机，直接报错（不允许半开半关起步）。
 */
static motion_phase_e start_door_motion(void)
{
    module_moto_pos_e door0_position;
    module_moto_pos_e door1_position;

    door0_position = module_moto_get_position(DOOR0_MOTOR_ID);
    door1_position = module_moto_get_position(DOOR1_MOTOR_ID);

    if ((door0_position == MODULE_MOTO_POS_ERROR) ||
        (door1_position == MODULE_MOTO_POS_ERROR))
    {
        stop_door_motors();
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
        return MOTION_ERR;
    }

    /* 双门均已在目标：无需动作 */
    if ((door0_position == door_motion.target_position) &&
        (door1_position == door_motion.target_position))
    {
        stop_door_motors();

        if (door_motion.target_position == MODULE_MOTO_POS_FORWARD)
        {
            run_status.door = MODULE_MOTO_DOOR_STATE_OPENED;
        }
        else if (door_motion.target_position == MODULE_MOTO_POS_BACKWARD)
        {
            run_status.door = MODULE_MOTO_DOOR_STATE_CLOSED;
        }

        return MOTION_OVER;
    }

    /* 任意单门已在目标：不启动，直接报错 */
    if ((door0_position == door_motion.target_position) ||
        (door1_position == door_motion.target_position))
    {
        stop_door_motors();
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
        return MOTION_ERR;
    }

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
    /* 新行程：边沿差与堵转计时从 0 开始 */
    module_moto_hall_reset_all_edge_counts();

    door0_block_monitor.last_edge_count = 0U;
    door0_block_monitor.no_edge_time_ms = 0U;

    door1_block_monitor.last_edge_count = 0U;
    door1_block_monitor.no_edge_time_ms = 0U;
#endif

    /* 双门紧挨启动，尽量缩短启动间隔；失败统一停机 */
    {
        bool door0_started;
        bool door1_started;

        door0_started = module_moto_start(DOOR0_MOTOR_ID, door_motion.direction);
        door1_started = module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);

        if (!door0_started || !door1_started)
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
 * @return 下一阶段：双门到位则 OVER，否则保持 ING
 *
 * 默认策略：
 * 1. 双门均到位 → 停机 OVER
 * 2. 单门到位 → 单独停止
 *
 * 开启门同步宏后，额外按霍尔边沿差调节双门，并检测堵转。
 */
static motion_phase_e update_door_motion(void)
{
    module_moto_pos_e door0_position;
    module_moto_pos_e door1_position;
#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
    uint32_t door0_edge_count;
    uint32_t door1_edge_count;
    uint32_t edge_count_diff;
    bool door0_blocked;
    bool door1_blocked;
#endif

    door0_position = module_moto_get_position(DOOR0_MOTOR_ID);
    door1_position = module_moto_get_position(DOOR1_MOTOR_ID);

    if ((door0_position == MODULE_MOTO_POS_ERROR) ||
        (door1_position == MODULE_MOTO_POS_ERROR))
    {
        stop_door_motors();
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;
        return MOTION_ERR;
    }

    /* 双门均到达目标限位 */
    if ((door0_position == door_motion.target_position) &&
        (door1_position == door_motion.target_position))
    {
        stop_door_motors();

        if (door_motion.target_position == MODULE_MOTO_POS_FORWARD)
        {
            run_status.door = MODULE_MOTO_DOOR_STATE_OPENED;
        }
        else if (door_motion.target_position == MODULE_MOTO_POS_BACKWARD)
        {
            run_status.door = MODULE_MOTO_DOOR_STATE_CLOSED;
        }

        return MOTION_OVER;
    }

#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
    door0_edge_count = module_moto_hall_get_edge_count(MODULE_MOTO_HALL_ID_DOOR0);
    door1_edge_count = module_moto_hall_get_edge_count(MODULE_MOTO_HALL_ID_DOOR1);

    /* 先停住已到位的门，避免顶限位继续出力 */
    if (door0_position == door_motion.target_position)
    {
        (void)module_moto_stop(DOOR0_MOTOR_ID);
    }

    if (door1_position == door_motion.target_position)
    {
        (void)module_moto_stop(DOOR1_MOTOR_ID);
    }

    door0_blocked = door_motor_block_timed_out(DOOR0_MOTOR_ID, door0_edge_count, &door0_block_monitor);
    door1_blocked = door_motor_block_timed_out(DOOR1_MOTOR_ID, door1_edge_count, &door1_block_monitor);

    if (door0_blocked || door1_blocked)
    {
        stop_door_motors();
        door_motion.motion_error = MOTION_ERROR_DOOR_BLOCKED;
        return MOTION_ERR;
    }

    if (door0_position == door_motion.target_position)
    {
        /* 门0 已到位：门1 独立跑完 */
        (void)module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);
    }
    else if (door1_position == door_motion.target_position)
    {
        /* 门1 已到位：门0 独立跑完 */
        (void)module_moto_start(DOOR0_MOTOR_ID, door_motion.direction);
    }
    else if (door0_edge_count >= door1_edge_count)
    {
        /* 门0 领先（或持平） */
        edge_count_diff = door0_edge_count - door1_edge_count;

        if (edge_count_diff >= DOOR_SYNC_STOP_EDGE_DIFF)
        {
            (void)module_moto_stop(DOOR0_MOTOR_ID);
            (void)module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);
        }
        else if (edge_count_diff < DOOR_SYNC_RESUME_EDGE_DIFF)
        {
            (void)module_moto_start(DOOR0_MOTOR_ID, door_motion.direction);
            (void)module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);
        }
        /* else: STOP~RESUME 死区，保持现状 */
    }
    else
    {
        /* 门1 领先 */
        edge_count_diff = door1_edge_count - door0_edge_count;

        if (edge_count_diff >= DOOR_SYNC_STOP_EDGE_DIFF)
        {
            (void)module_moto_start(DOOR0_MOTOR_ID, door_motion.direction);
            (void)module_moto_stop(DOOR1_MOTOR_ID);
        }
        else if (edge_count_diff < DOOR_SYNC_RESUME_EDGE_DIFF)
        {
            (void)module_moto_start(DOOR0_MOTOR_ID, door_motion.direction);
            (void)module_moto_start(DOOR1_MOTOR_ID, door_motion.direction);
        }
        /* else: STOP~RESUME 死区，保持现状 */
    }

    return MOTION_ING;
#else
    if (door0_position == door_motion.target_position)
    {
        (void)module_moto_stop(DOOR0_MOTOR_ID);
    }

    if (door1_position == door_motion.target_position)
    {
        (void)module_moto_stop(DOOR1_MOTOR_ID);
    }

    return MOTION_ING;
#endif
}

/**
 * @brief  启动停机坪运动（MOTION_START 阶段处理）。
 * @return 下一阶段：已到位 OVER；启动成功 ING；启动失败 ERR
 */
static motion_phase_e start_heliped_motion(void)
{
    module_moto_pos_e heliped_position;

    heliped_position = module_moto_get_position(HELIPED_MOTOR_ID);

    if (heliped_position == MODULE_MOTO_POS_ERROR)
    {
        (void)module_moto_stop(HELIPED_MOTOR_ID);
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_FAULT;
        return MOTION_ERR;
    }

    if (heliped_position == heliped_motion.target_position)
    {
        (void)module_moto_stop(HELIPED_MOTOR_ID);

        if (heliped_motion.target_position == MODULE_MOTO_POS_FORWARD)
        {
            run_status.heliped = MODULE_MOTO_HELIPED_STATE_EXTENDED;
        }
        else if (heliped_motion.target_position == MODULE_MOTO_POS_BACKWARD)
        {
            run_status.heliped = MODULE_MOTO_HELIPED_STATE_RETRACTED;
        }

        return MOTION_OVER;
    }

    if (!module_moto_start(HELIPED_MOTOR_ID, heliped_motion.direction))
    {
        (void)module_moto_stop(HELIPED_MOTOR_ID);
        heliped_motion.start_err_cnt++;
    }
    else
    {
        heliped_motion.start_err_cnt = 0U;
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_INTERMEDIATE;
        return MOTION_ING;
    }

    if(heliped_motion.start_err_cnt >= 3U)
    {
        heliped_motion.start_err_cnt = 0U;
        heliped_motion.motion_error = MOTION_ERROR_HELIPED_START_FAILED;
        return MOTION_ERR;
    }

    return MOTION_START;
}

/**
 * @brief  停机坪运行期更新（MOTION_ING 阶段处理）。
 * @return 到位则停机 OVER，否则保持 ING
 * @note   当前无堵转/总超时保护，依赖限位反馈结束。
 */
static motion_phase_e update_heliped_motion(void)
{
    module_moto_pos_e heliped_position;

    heliped_position = module_moto_get_position(HELIPED_MOTOR_ID);

    if (heliped_position == MODULE_MOTO_POS_ERROR)
    {
        (void)module_moto_stop(HELIPED_MOTOR_ID);
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_FAULT;
        return MOTION_ERR;
    }

    if (heliped_position != heliped_motion.target_position)
    {
        return MOTION_ING;
    }

    (void)module_moto_stop(HELIPED_MOTOR_ID);

    if (heliped_motion.target_position == MODULE_MOTO_POS_FORWARD)
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_EXTENDED;
    }
    else if (heliped_motion.target_position == MODULE_MOTO_POS_BACKWARD)
    {
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_RETRACTED;
    }

    return MOTION_OVER;
}

/**
 * @brief  门故障处理（MOTION_ERR 阶段）。
 * @return 保持 MOTION_ERR
 * @note   按故障码校验门电机方向；不在预计状态则纠正控制。
 *         START_FAILED / BLOCKED：双门应为 STOP。
 */
static motion_phase_e handle_door_motion_error(void)
{
    switch (door_motion.motion_error)
    {
    case MOTION_ERROR_DOOR_START_FAILED:
#if MODULE_MOTO_HALL_ENABLE && MODULE_MOTO_DOOR_SYNC_ENABLE
    case MOTION_ERROR_DOOR_BLOCKED:
#endif
        run_status.door = MODULE_MOTO_DOOR_STATE_FAULT;

        if ((module_moto_get_direction(DOOR0_MOTOR_ID) != MODULE_MOTO_DIR_STOP) ||
            (module_moto_get_direction(DOOR1_MOTOR_ID) != MODULE_MOTO_DIR_STOP))
        {
            stop_door_motors();
        }
        return MOTION_OVER;

    default:
        return MOTION_OVER;
    }

    return MOTION_ERR;
}

/**
 * @brief  停机坪故障处理（MOTION_ERR 阶段）。
 * @return 错误处理完成后进入 MOTION_OVER
 * @note   启动失败时停机坪电机应为 STOP。
 */
static motion_phase_e handle_heliped_motion_error(void)
{
    switch (heliped_motion.motion_error)
    {
    case MOTION_ERROR_HELIPED_START_FAILED:
        run_status.heliped = MODULE_MOTO_HELIPED_STATE_FAULT;

        if (module_moto_get_direction(HELIPED_MOTOR_ID) != MODULE_MOTO_DIR_STOP)
        {
            (void)module_moto_stop(HELIPED_MOTOR_ID);
        }
        return MOTION_OVER;

    default:
        return MOTION_OVER;
    }
}

void module_moto_sync_init(void)
{
    refresh_door_run_status();
    refresh_heliped_run_status();
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
    door_motion.phase = MOTION_OVER;
    door_motion.direction = MODULE_MOTO_DIR_STOP;
    door_motion.target_position = MODULE_MOTO_POS_NONE;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
    refresh_door_run_status();
}

void module_moto_sync_stop_heliped(void)
{
    (void)module_moto_stop(HELIPED_MOTOR_ID);
    heliped_motion.phase = MOTION_OVER;
    heliped_motion.direction = MODULE_MOTO_DIR_STOP;
    heliped_motion.target_position = MODULE_MOTO_POS_NONE;
    heliped_motion.motion_error = MOTION_ERROR_NONE;
    heliped_motion.start_err_cnt = 0U;
    refresh_heliped_run_status();
}

void module_moto_sync_request_door_open(void)
{
    door_motion.phase = MOTION_START;
    door_motion.direction = MODULE_MOTO_DIR_FORWARD;
    door_motion.target_position = MODULE_MOTO_POS_FORWARD;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
}

void module_moto_sync_request_door_close(void)
{
    door_motion.phase = MOTION_START;
    door_motion.direction = MODULE_MOTO_DIR_BACKWARD;
    door_motion.target_position = MODULE_MOTO_POS_BACKWARD;
    door_motion.motion_error = MOTION_ERROR_NONE;
    door_motion.start_err_cnt = 0U;
}

void module_moto_sync_request_heliped_extend(void)
{
    heliped_motion.phase = MOTION_START;
    heliped_motion.direction = MODULE_MOTO_DIR_FORWARD;
    heliped_motion.target_position = MODULE_MOTO_POS_FORWARD;
    heliped_motion.motion_error = MOTION_ERROR_NONE;
    heliped_motion.start_err_cnt = 0U;
}

void module_moto_sync_request_heliped_retract(void)
{
    heliped_motion.phase = MOTION_START;
    heliped_motion.direction = MODULE_MOTO_DIR_BACKWARD;
    heliped_motion.target_position = MODULE_MOTO_POS_BACKWARD;
    heliped_motion.motion_error = MOTION_ERROR_NONE;
    heliped_motion.start_err_cnt = 0U;
}

void module_moto_sync_update(void)
{
    /* 双门状态机 */
    switch (door_motion.phase)
    {
    case MOTION_START:
        door_motion.phase = start_door_motion();
        break;

    case MOTION_ING:
        door_motion.phase = update_door_motion();
        break;

    case MOTION_OVER:
        break;

    case MOTION_ERR:
        door_motion.phase = handle_door_motion_error();
        break;
    }

    /* 停机坪状态机（与门独立并行） */
    switch (heliped_motion.phase)
    {
    case MOTION_START:
        heliped_motion.phase = start_heliped_motion();
        break;

    case MOTION_ING:
        heliped_motion.phase = update_heliped_motion();
        break;

    case MOTION_OVER:
        break;

    case MOTION_ERR:
        heliped_motion.phase = handle_heliped_motion_error();
        break;
    }
}
