#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifndef MOTOR_CONTROL_PERIOD_MS
#define MOTOR_CONTROL_PERIOD_MS 10U
#endif

typedef enum {
    MOTOR_DOOR_INTERMEDIATE = 0,
    MOTOR_DOOR_CLOSED,
    MOTOR_DOOR_OPENED
} motor_door_position_e;

typedef enum {
    MOTOR_PLATFORM_INTERMEDIATE = 0,
    MOTOR_PLATFORM_EXTENDED,
    MOTOR_PLATFORM_RETRACTED
} motor_platform_position_e;

typedef enum {
    MOTOR_DOOR_STOPPED = 0,
    MOTOR_DOOR_OPENING,
    MOTOR_DOOR_CLOSING
} motor_door_action_e;

typedef enum {
    MOTOR_PLATFORM_STOPPED = 0,
    MOTOR_PLATFORM_EXTENDING,
    MOTOR_PLATFORM_RETRACTING
} motor_platform_action_e;

/* 位标志：限位恢复自动清除；启动失败允许新命令重试。 */
typedef enum {
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_LIMIT_CONFLICT = 1,
    MOTOR_FAULT_START_FAILED = 2
} motor_fault_e;

typedef struct {
    motor_door_position_e door_position;
    motor_door_action_e door_action;
    motor_fault_e door_fault;
    motor_platform_position_e platform_position;
    motor_platform_action_e platform_action;
    motor_fault_e platform_fault;
    bool key_control_active; /* 含联动等待及暂停；104 不得接管门/平台。 */
    bool paused;
    bool test_mode; /* 仅取消门/平台联动与位置互锁；上电默认关闭。 */
} motor_control_status_t;

/* 初始化板级输入/锁和单电机驱动，建立门/平台状态。 */
void motor_control_init(void);
/* 实际切换模式时停止全部电机并清除意图；重复设置当前模式无动作。 */
void motor_control_set_test_mode(bool enabled);
void motor_control_request_door_open(void);
void motor_control_request_door_close(void);
void motor_control_request_platform_extend(void);
void motor_control_request_platform_retract(void);
/* 发起键短按暂停/继续整套流程；另一键提交新目标，中间位置默认关门/收平台。 */
void motor_control_toggle_door(void);
void motor_control_toggle_platform(void);
/* 急停取消两机构的全部请求；释放不续跑。 */
void motor_control_set_estop(bool active);
/* 默认每 10 ms 调用，调用前由应用读取物理急停。 */
void motor_control_update(void);
/* 只复制控制层快照，无硬件读写或停机副作用。action 仅表示实际运动。 */
void motor_control_get_status(motor_control_status_t *status);

#endif /* MOTOR_CONTROL_H */
