#ifndef __BOARD_IO_H__
#define __BOARD_IO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 板级配置（当前资源枚举） */
#include "tf_board_config.h"
/* dev 层接口 */
#include "tf_drv_io.h"

extern tf_drv_io_config_t io_configs[E_IO_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_IO_H__ */
