/**
 * @file    tf_drv_i2c.c
 * @brief   I2C 驱动实现（配置 + 主模式事务）
 */

#include "tf_drv_i2c.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_i2c.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_I2C

#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif


/* ========== 运行时状态 ========== */

static bool i2c_registered[E_DRV_I2C_MAX];

/* ========== 内部辅助 ========== */

/** 从句柄获取 id，验证注册状态 */
static int i2c_get_id(tf_drv_i2c_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_i2c_config_t *)h - i2c_configs;
    if (id < 0 || id >= E_DRV_I2C_MAX || !i2c_registered[id])
        return -1;
    return id;
}

static void hal_init_common(int id)
{
    const tf_drv_i2c_config_t *c = &i2c_configs[id];

    tf_hal_i2c_init(c->base);
}

tf_drv_i2c_handle_t tf_drv_i2c_register(int id)
{
    TF_ASSERT(id >= 0 && id < E_DRV_I2C_MAX);
    if (id < 0 || id >= E_DRV_I2C_MAX) return NULL;

    if (i2c_registered[id]) return (tf_drv_i2c_handle_t)&i2c_configs[id];

    const tf_drv_i2c_config_t *cfg = &i2c_configs[id];

    if (cfg->base == NULL) {
        return NULL;
    }

    if (cfg->io.scl_id >= 0) tf_drv_io_register(cfg->io.scl_id);
    if (cfg->io.sda_id >= 0) tf_drv_io_register(cfg->io.sda_id);

    hal_init_common(id);

    i2c_registered[id] = true;
    return (tf_drv_i2c_handle_t)&i2c_configs[id];
}

tf_drv_i2c_handle_t tf_drv_i2c_get_handle(int id)
{
    if (id < 0 || id >= E_DRV_I2C_MAX) return NULL;
    return i2c_registered[id] ? (tf_drv_i2c_handle_t)&i2c_configs[id] : NULL;
}

bool tf_drv_i2c_is_registered(int id)
{
    return (id >= 0 && id < E_DRV_I2C_MAX) ? i2c_registered[id] : false;
}

static int i2c_get_base(tf_drv_i2c_handle_t h, void **base)
{
    int id = i2c_get_id(h);
    if (id < 0) return -1;
    *base = i2c_configs[id].base;
    return 0;
}

int tf_drv_i2c_master_tx(tf_drv_i2c_handle_t h, uint8_t addr, const uint8_t *data, uint32_t len)
{
}

int tf_drv_i2c_master_rx(tf_drv_i2c_handle_t h, uint8_t addr, uint8_t *buf, uint32_t len)
{
}

int tf_drv_i2c_master_tx_rx(tf_drv_i2c_handle_t h, uint8_t addr,
                             const uint8_t *tx_data, uint32_t tx_len,
                             uint8_t *rx_buf, uint32_t rx_len)
{
             (void *)h, addr, (unsigned long)tx_len, (unsigned long)rx_len);
    TF_ASSERT(h != NULL); if (h == NULL) return -1;
    TF_ASSERT(tx_data != NULL || tx_len == 0); if (tx_data == NULL && tx_len > 0) return -1;
    TF_ASSERT(rx_buf != NULL || rx_len == 0); if (rx_buf == NULL && rx_len > 0) return -1;

}
#endif /* MCU_CONTAIN_PRE_I2C */
