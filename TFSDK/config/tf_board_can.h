#ifndef __BOARD_CAN_H__
#define __BOARD_CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tf_board_config.h"
#include "tf_drv_can.h"

extern tf_drv_can_config_t can_configs[E_DRV_CAN_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CAN_H__ */
