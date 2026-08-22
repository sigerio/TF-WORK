#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODULE_MOTO_FB_ACTIVE_LEVEL
#define MODULE_MOTO_FB_ACTIVE_LEVEL 0U
#endif

typedef enum {
    PLATFORM_MOTOR_ID = 0, /* 停机坪推杆电机。 */
    DOOR1_MOTOR_ID,        /* 左门电机。 */
    DOOR2_MOTOR_ID,        /* 右门电机。 */

    MODULE_MOTO_ID_COUNT
} module_moto_id_e;

typedef enum {
    MODULE_MOTO_DIR_STOP = 0, /* 两个驱动通道均关闭。 */
    MODULE_MOTO_DIR_FORWARD,  /* 伸出/开门方向：仅正向通道输出。 */
    MODULE_MOTO_DIR_BACKWARD, /* 收回/关门方向：仅反向通道输出。 */
    MODULE_MOTO_DIR_BRAKE     /* 两个驱动通道均使能，用于锁住电机。 */
} module_moto_dir_e;

typedef enum {
    MODULE_MOTO_POS_NONE = 0,  /* 两个到位反馈均未触发。 */
    MODULE_MOTO_POS_FORWARD,   /* 仅正向到位反馈触发。 */
    MODULE_MOTO_POS_BACKWARD,  /* 仅反向到位反馈触发。 */
    MODULE_MOTO_POS_ERROR      /* 两个到位反馈同时触发或电机无效。 */
} module_moto_pos_e;

typedef struct {
    uint8_t extend_output_active;   /* 正向 PWM 通道是否已使能。 */
    uint8_t retract_output_active;  /* 反向 PWM 通道是否已使能。 */
    uint8_t extend_feedback_active; /* 正向到位反馈是否有效。 */
    uint8_t retract_feedback_active;/* 反向到位反馈是否有效。 */
} module_moto_driver_status_t;

/* 初始化电机 PWM、反馈输入和门电吸锁输出。 */
bool module_moto_init(void);
/* 驱动指定电机；direction 为 FORWARD 或 BACKWARD 时只使能一个通道。 */
bool module_moto_start(module_moto_id_e motor_id, module_moto_dir_e direction);
/* 停止时关闭两个 PWM 通道；刹车时同时使能两个通道。 */
bool module_moto_stop(module_moto_id_e motor_id);
bool module_moto_brake(module_moto_id_e motor_id);
/* 对全部三台电机执行停止，不执行刹车。 */
void module_moto_stop_all(void);

/* 读取单电机到位反馈、归纳位置和当前驱动方向。 */
bool module_moto_is_at_forward(module_moto_id_e motor_id);
bool module_moto_is_at_backward(module_moto_id_e motor_id);
module_moto_pos_e module_moto_get_position(module_moto_id_e motor_id);
module_moto_dir_e module_moto_get_direction(module_moto_id_e motor_id);
void module_moto_get_driver_status(module_moto_id_e motor_id,
                                   module_moto_driver_status_t *status);

/* 门电吸锁为低有效：lock 输出低电平，unlock 输出高电平。 */
void module_moto_door_lock(void);
void module_moto_door_unlock(void);

/* 平台、门组公共限位均为读低触发。 */
bool module_moto_platform_extend_limit_active(void);
bool module_moto_platform_retract_limit_active(void);
bool module_moto_door_open_limit_active(void);
bool module_moto_door_close_limit_active(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_H */
