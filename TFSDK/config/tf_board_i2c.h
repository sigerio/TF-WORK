#ifndef __BOARD_I2C_H__
#define __BOARD_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tf_board_config.h"
#include "tf_drv_i2c.h"

extern tf_drv_i2c_config_t i2c_configs[E_DRV_I2C_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_I2C_H__ */
