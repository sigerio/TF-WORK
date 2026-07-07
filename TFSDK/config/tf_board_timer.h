#ifndef __BOARD_TIMER_H__
#define __BOARD_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 板级配置（当前资源枚举） */
#include "tf_board_config.h"
/* dev 层接口 */
#include "tf_drv_timer.h"

extern tf_drv_timer_config_t timer_configs[E_DRV_TIMER_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_TIMER_H__ */
