#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_PLATFORM = 0,
    MOTOR_DOOR1,
    MOTOR_DOOR2,
    MOTOR_ID_COUNT
} motor_id_e;

typedef enum {
    MOTOR_DIR_STOP = 0,
    MOTOR_DIR_FORWARD,
    MOTOR_DIR_BACKWARD,
    MOTOR_DIR_BRAKE
} motor_direction_e;

/* 仅初始化 PWM，所有通道保持关闭；不读取限位或控制门锁。 */
bool motor_driver_init(void);
bool motor_driver_start(motor_id_e motor_id, motor_direction_e direction);
bool motor_driver_stop(motor_id_e motor_id);
bool motor_driver_brake(motor_id_e motor_id);
motor_direction_e motor_driver_get_direction(motor_id_e motor_id);

#endif /* MOTOR_DRIVER_H */
