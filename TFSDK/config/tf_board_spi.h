#ifndef __BOARD_SPI_H__
#define __BOARD_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 板级配置（当前资源枚举） */
#include "tf_board_config.h"
/* dev 层接口 */
#include "tf_drv_spi.h"

extern tf_drv_spi_config_t spi_configs[E_DRV_SPI_MAX];

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_SPI_H__ */
