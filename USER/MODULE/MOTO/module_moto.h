#ifndef MODULE_MOTO_H
#define MODULE_MOTO_H

#include <stdint.h>

#include "tf_board_config.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODULE_MOTO_FB_ACTIVE_LEVEL
#define MODULE_MOTO_FB_ACTIVE_LEVEL 0U
#endif

typedef enum {
    HELIPED_MOTOR_ID = 0,
    DOOR0_MOTOR_ID,
    DOOR1_MOTOR_ID,

    MODULE_MOTO_ID_COUNT
} module_moto_id_e;

typedef enum {
    MODULE_MOTO_DIR_STOP = 0,
    MODULE_MOTO_DIR_FORWARD,
    MODULE_MOTO_DIR_BACKWARD,
    MODULE_MOTO_DIR_BRAKE
} module_moto_dir_e;

typedef enum {
    MODULE_MOTO_POS_NONE = 0,
    MODULE_MOTO_POS_FORWARD,
    MODULE_MOTO_POS_BACKWARD,
    MODULE_MOTO_POS_ERROR
} module_moto_pos_e;

bool module_moto_init(void);
bool module_moto_start(module_moto_id_e motor_id, module_moto_dir_e direction);
bool module_moto_stop(module_moto_id_e motor_id);
bool module_moto_brake(module_moto_id_e motor_id);
void module_moto_stop_all(void);

bool module_moto_is_at_forward(module_moto_id_e motor_id);
bool module_moto_is_at_backward(module_moto_id_e motor_id);
module_moto_pos_e module_moto_get_position(module_moto_id_e motor_id);
module_moto_dir_e module_moto_get_direction(module_moto_id_e motor_id);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MOTO_H */
