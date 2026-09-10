#include "CONTROL/motor/motor_control.h"
#include "CONTROL/motor/motor_driver.h"
#include "BOARD/board.h"
#include "OS/os.h"
#include "APP/app_status.h"
#include "APP/app_events.h"
#include "APP/ws2812_app.h"

#define MOTOR_REVERSE_WAIT_MS 100U

typedef enum {
    DOOR_IDLE = 0,
    DOOR_START_OPEN,
    DOOR_OPEN_RUNNING,
    DOOR_WAIT_PLATFORM,
    DOOR_START_CLOSE,
    DOOR_CLOSE_RUNNING,
    DOOR_REVERSE_TO_OPEN,
    DOOR_REVERSE_TO_CLOSE
} door_phase_e;

static door_phase_e door_phase;
static uint16_t door_reverse_ms;
static uint8_t door_start_failures;

typedef enum {
    PLATFORM_IDLE = 0,
    PLATFORM_START_RETRACT,
    PLATFORM_RETRACT_RUNNING,
    PLATFORM_WAIT_DOOR,
    PLATFORM_START_EXTEND,
    PLATFORM_EXTEND_RUNNING,
    PLATFORM_REVERSE_TO_RETRACT,
    PLATFORM_REVERSE_TO_EXTEND
} platform_phase_e;

static platform_phase_e platform_phase;
static uint16_t platform_reverse_ms;
static uint8_t platform_start_failures;

static bool estop_active;
/* 保留具名符号供调试器 Watch 查看；外部通过只读快照接口访问。 */
static motor_control_status_t motor_status;

static void read_positions_and_faults(void)
{
    board_motor_limits_t limits;

    board_motor_read_limits(&limits);
    motor_status.door_fault &= ~MOTOR_FAULT_LIMIT_CONFLICT;
    motor_status.platform_fault &= ~MOTOR_FAULT_LIMIT_CONFLICT;
    motor_status.door_position = MOTOR_DOOR_INTERMEDIATE;
    motor_status.platform_position = MOTOR_PLATFORM_INTERMEDIATE;
    if (limits.door_closed && limits.door_opened) motor_status.door_fault |= MOTOR_FAULT_LIMIT_CONFLICT;
    else if (limits.door_closed) motor_status.door_position = MOTOR_DOOR_CLOSED;
    else if (limits.door_opened) motor_status.door_position = MOTOR_DOOR_OPENED;
    if (limits.platform_extended && limits.platform_retracted) motor_status.platform_fault |= MOTOR_FAULT_LIMIT_CONFLICT;
    else if (limits.platform_extended) motor_status.platform_position = MOTOR_PLATFORM_EXTENDED;
    else if (limits.platform_retracted) motor_status.platform_position = MOTOR_PLATFORM_RETRACTED;
}

static void stop_door_motors(void)
{
    (void)motor_driver_stop(MOTOR_DOOR1);
    (void)motor_driver_stop(MOTOR_DOOR2);
}

static void stop_door_motion(void)
{
    door_running_in_e running = E_DOOR_RUN_IDLE;

    stop_door_motors();
    if (is_signal_set(OS_DOOR_S)) (void)os_event_trigger(USR_EVENT_DOOR_STATE, &running, sizeof(running), 1U);
    cancel_signal(OS_DOOR_S);
    door_phase = DOOR_IDLE;
    door_reverse_ms = 0U;
    door_start_failures = 0U;
    motor_status.door_action = MOTOR_DOOR_STOPPED;
    motor_status.door_fault &= ~MOTOR_FAULT_START_FAILED;
    if (motor_status.door_position == MOTOR_DOOR_CLOSED) board_door_lock_write(true);
}

static void stop_platform_motion(void)
{
    platform_running_in_e running = E_PLATFORM_RUN_IDLE;

    (void)motor_driver_stop(MOTOR_PLATFORM);
    if (is_signal_set(OS_HELIPAD_S)) (void)os_event_trigger(USR_EVENT_PLATFORM_STATE, &running, sizeof(running), 1U);
    cancel_signal(OS_HELIPAD_S);
    platform_phase = PLATFORM_IDLE;
    platform_reverse_ms = 0U;
    platform_start_failures = 0U;
    motor_status.platform_action = MOTOR_PLATFORM_STOPPED;
    motor_status.platform_fault &= ~MOTOR_FAULT_START_FAILED;
}

/* 命令与周期共用保护入口：故障出现/恢复都取消旧请求，查询接口不做这些操作。 */
static bool enforce_limit_stop(void)
{
    bool was_fault = ((motor_status.door_fault | motor_status.platform_fault) & MOTOR_FAULT_LIMIT_CONFLICT) != 0;
    bool fault;

    read_positions_and_faults();
    fault = ((motor_status.door_fault | motor_status.platform_fault) & MOTOR_FAULT_LIMIT_CONFLICT) != 0;
    if (was_fault || fault) {
        stop_door_motion();
        stop_platform_motion();
    }
    return fault;
}

void motor_control_set_estop(bool active)
{
    if (estop_active == active) return;
    estop_active = active;
    if (active) {
        read_positions_and_faults();
        stop_door_motion();
        stop_platform_motion();
    }
}

void motor_control_init(void)
{
    board_motor_io_init();
    (void)motor_driver_init();
    if (enforce_limit_stop()) return;
    (void)motor_driver_brake(MOTOR_DOOR1);
    (void)motor_driver_brake(MOTOR_DOOR2);
    stop_door_motion();
    stop_platform_motion();
}

void motor_control_get_status(motor_control_status_t *status)
{
    if (status != 0) *status = motor_status;
}

static void queue_door_motion(door_phase_e phase)
{
    door_running_in_e running = E_DOOR_RUN_IN_ACTION;

    door_phase = phase;
    door_start_failures = 0U;
    motor_status.door_fault &= ~MOTOR_FAULT_START_FAILED;
    set_signal(OS_DOOR_S);
    (void)os_event_trigger(USR_EVENT_DOOR_STATE, &running, sizeof(running), 1U);
    if (phase == DOOR_START_OPEN) board_door_lock_write(false);
}

void motor_control_request_door_open(void)
{
    if (enforce_limit_stop() || estop_active) return;
    switch (door_phase) {
    case DOOR_START_OPEN:
    case DOOR_OPEN_RUNNING:
    case DOOR_REVERSE_TO_OPEN:
        return;
    case DOOR_START_CLOSE:
    case DOOR_CLOSE_RUNNING:
        stop_door_motion();
        door_phase = DOOR_REVERSE_TO_OPEN;
        return;
    case DOOR_REVERSE_TO_CLOSE:
        door_phase = DOOR_REVERSE_TO_OPEN;
        return;
    default:
        queue_door_motion(DOOR_START_OPEN);
        return;
    }
}

void motor_control_request_door_close(void)
{
    if (enforce_limit_stop() || estop_active) return;
    if (door_phase == DOOR_WAIT_PLATFORM || door_phase == DOOR_START_CLOSE ||
        door_phase == DOOR_CLOSE_RUNNING || door_phase == DOOR_REVERSE_TO_CLOSE) return;

    motor_control_request_platform_retract();
    switch (door_phase) {
    case DOOR_START_OPEN:
    case DOOR_OPEN_RUNNING:
        stop_door_motion();
        door_phase = DOOR_REVERSE_TO_CLOSE;
        break;
    case DOOR_REVERSE_TO_OPEN:
        door_phase = DOOR_REVERSE_TO_CLOSE;
        break;
    default:
        door_phase = DOOR_WAIT_PLATFORM;
        break;
    }
}

void motor_control_toggle_door(void)
{
    if (enforce_limit_stop() || estop_active) return;
    switch (door_phase) {
    case DOOR_START_OPEN:
    case DOOR_OPEN_RUNNING:
    case DOOR_REVERSE_TO_OPEN:
        motor_control_request_door_close();
        break;
    case DOOR_WAIT_PLATFORM:
    case DOOR_START_CLOSE:
    case DOOR_CLOSE_RUNNING:
    case DOOR_REVERSE_TO_CLOSE:
        motor_control_request_door_open();
        break;
    default:
        if (motor_status.door_position == MOTOR_DOOR_OPENED) motor_control_request_door_close();
        else motor_control_request_door_open();
        break;
    }
}

static bool door_close_allowed(void)
{
    return motor_status.platform_fault == MOTOR_FAULT_NONE && motor_status.platform_position == MOTOR_PLATFORM_RETRACTED &&
        (platform_phase == PLATFORM_IDLE || platform_phase == PLATFORM_START_RETRACT || platform_phase == PLATFORM_RETRACT_RUNNING);
}

static void update_door(void)
{
    switch (door_phase) {
    case DOOR_REVERSE_TO_OPEN:
    case DOOR_REVERSE_TO_CLOSE:
        door_reverse_ms += MOTOR_CONTROL_PERIOD_MS;
        if (door_reverse_ms < MOTOR_REVERSE_WAIT_MS) break;
        door_reverse_ms = 0U;
        if (door_phase == DOOR_REVERSE_TO_OPEN) queue_door_motion(DOOR_START_OPEN);
        else if (door_close_allowed()) queue_door_motion(DOOR_START_CLOSE);
        else door_phase = DOOR_WAIT_PLATFORM;
        break;
    case DOOR_WAIT_PLATFORM:
        if (door_close_allowed()) queue_door_motion(DOOR_START_CLOSE);
        break;
    case DOOR_START_OPEN:
    case DOOR_OPEN_RUNNING:
    case DOOR_START_CLOSE:
    case DOOR_CLOSE_RUNNING: {
        bool open = door_phase == DOOR_START_OPEN || door_phase == DOOR_OPEN_RUNNING;
        bool at_target = motor_status.door_position == (open ? MOTOR_DOOR_OPENED : MOTOR_DOOR_CLOSED);
        bool started;

        if (at_target) {
            stop_door_motion();
            break;
        }
        if (door_phase == DOOR_OPEN_RUNNING || door_phase == DOOR_CLOSE_RUNNING) break;
        /* 两台电机共用门组限位；任何一台启动失败都停止整个门组。 */
        started = motor_driver_start(MOTOR_DOOR1, open ? MOTOR_DIR_BACKWARD : MOTOR_DIR_FORWARD);
        if (!motor_driver_start(MOTOR_DOOR2, open ? MOTOR_DIR_BACKWARD : MOTOR_DIR_FORWARD)) started = false;
        if (started) {
            door_start_failures = 0U;
            door_phase = open ? DOOR_OPEN_RUNNING : DOOR_CLOSE_RUNNING;
            motor_status.door_action = open ? MOTOR_DOOR_OPENING : MOTOR_DOOR_CLOSING;
        } else {
            stop_door_motors();
            if (++door_start_failures >= 3U) {
                stop_door_motion();
                motor_status.door_fault |= MOTOR_FAULT_START_FAILED;
            }
        }
        break;
    }
    case DOOR_IDLE:
    default:
        break;
    }
}

static void queue_platform_motion(platform_phase_e phase)
{
    platform_running_in_e running = E_PLATFORM_RUN_IN_ACTION;

    platform_phase = phase;
    platform_start_failures = 0U;
    motor_status.platform_fault &= ~MOTOR_FAULT_START_FAILED;
    set_signal(OS_HELIPAD_S);
    (void)os_event_trigger(USR_EVENT_PLATFORM_STATE, &running, sizeof(running), 1U);
}

void motor_control_request_platform_retract(void)
{
    if (enforce_limit_stop() || estop_active) return;
    switch (platform_phase) {
    case PLATFORM_START_RETRACT:
    case PLATFORM_RETRACT_RUNNING:
    case PLATFORM_REVERSE_TO_RETRACT:
        return;
    case PLATFORM_START_EXTEND:
    case PLATFORM_EXTEND_RUNNING:
        stop_platform_motion();
        platform_phase = PLATFORM_REVERSE_TO_RETRACT;
        return;
    case PLATFORM_REVERSE_TO_EXTEND:
        platform_phase = PLATFORM_REVERSE_TO_RETRACT;
        return;
    default:
        queue_platform_motion(PLATFORM_START_RETRACT);
        return;
    }
}

void motor_control_request_platform_extend(void)
{
    if (enforce_limit_stop() || estop_active) return;
    if (platform_phase == PLATFORM_WAIT_DOOR || platform_phase == PLATFORM_START_EXTEND ||
        platform_phase == PLATFORM_EXTEND_RUNNING || platform_phase == PLATFORM_REVERSE_TO_EXTEND) return;

    motor_control_request_door_open();
    switch (platform_phase) {
    case PLATFORM_START_RETRACT:
    case PLATFORM_RETRACT_RUNNING:
        stop_platform_motion();
        platform_phase = PLATFORM_REVERSE_TO_EXTEND;
        break;
    case PLATFORM_REVERSE_TO_RETRACT:
        platform_phase = PLATFORM_REVERSE_TO_EXTEND;
        break;
    default:
        platform_phase = PLATFORM_WAIT_DOOR;
        break;
    }
}

void motor_control_toggle_platform(void)
{
    if (enforce_limit_stop() || estop_active) return;
    switch (platform_phase) {
    case PLATFORM_START_RETRACT:
    case PLATFORM_RETRACT_RUNNING:
    case PLATFORM_REVERSE_TO_RETRACT:
        motor_control_request_platform_extend();
        break;
    case PLATFORM_WAIT_DOOR:
    case PLATFORM_START_EXTEND:
    case PLATFORM_EXTEND_RUNNING:
    case PLATFORM_REVERSE_TO_EXTEND:
        motor_control_request_platform_retract();
        break;
    default:
        if (motor_status.platform_position == MOTOR_PLATFORM_EXTENDED) motor_control_request_platform_retract();
        else motor_control_request_platform_extend();
        break;
    }
}

static bool platform_extend_allowed(void)
{
    return motor_status.door_fault == MOTOR_FAULT_NONE && motor_status.door_position == MOTOR_DOOR_OPENED &&
        (door_phase == DOOR_IDLE || door_phase == DOOR_START_OPEN || door_phase == DOOR_OPEN_RUNNING);
}

static void update_platform(void)
{
    switch (platform_phase) {
    case PLATFORM_REVERSE_TO_RETRACT:
    case PLATFORM_REVERSE_TO_EXTEND:
        platform_reverse_ms += MOTOR_CONTROL_PERIOD_MS;
        if (platform_reverse_ms < MOTOR_REVERSE_WAIT_MS) break;
        platform_reverse_ms = 0U;
        if (platform_phase == PLATFORM_REVERSE_TO_RETRACT) queue_platform_motion(PLATFORM_START_RETRACT);
        else if (platform_extend_allowed()) queue_platform_motion(PLATFORM_START_EXTEND);
        else platform_phase = PLATFORM_WAIT_DOOR;
        break;
    case PLATFORM_WAIT_DOOR:
        if (platform_extend_allowed()) queue_platform_motion(PLATFORM_START_EXTEND);
        break;
    case PLATFORM_START_RETRACT:
    case PLATFORM_RETRACT_RUNNING:
    case PLATFORM_START_EXTEND:
    case PLATFORM_EXTEND_RUNNING: {
        bool retract = platform_phase == PLATFORM_START_RETRACT || platform_phase == PLATFORM_RETRACT_RUNNING;
        bool at_target = motor_status.platform_position == (retract ? MOTOR_PLATFORM_RETRACTED : MOTOR_PLATFORM_EXTENDED);
        bool started;

        if (at_target) {
            stop_platform_motion();
            break;
        }
        if (platform_phase == PLATFORM_RETRACT_RUNNING || platform_phase == PLATFORM_EXTEND_RUNNING) break;
        started = motor_driver_start(MOTOR_PLATFORM, retract ? MOTOR_DIR_BACKWARD : MOTOR_DIR_FORWARD);
        if (started) {
            platform_start_failures = 0U;
            platform_phase = retract ? PLATFORM_RETRACT_RUNNING : PLATFORM_EXTEND_RUNNING;
            motor_status.platform_action = retract ? MOTOR_PLATFORM_RETRACTING : MOTOR_PLATFORM_EXTENDING;
        } else {
            (void)motor_driver_stop(MOTOR_PLATFORM);
            if (++platform_start_failures >= 3U) {
                stop_platform_motion();
                motor_status.platform_fault |= MOTOR_FAULT_START_FAILED;
            }
        }
        break;
    }
    case PLATFORM_IDLE:
    default:
        break;
    }
}

void motor_control_update(void)
{
    if (enforce_limit_stop() || estop_active) return;
    /* 门先推进，平台在同一周期读取门的最新阶段。 */
    update_door();
    update_platform();
}
