/**
 * @file    tf_drv_spi.c
 * @brief   SPI 外设注册与数据收发实现
 *
 * 实现要点：
 *   - 用户必须提供 spi_configs[] 数组（命名固定）
 *   - 所有硬件操作委托给 tf_hal_spi.h 中的内联函数
 *   - 内部函数均为 static
 */

#include "tf_drv_spi.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_spi.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_SPI

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif


/* ========== 运行时状态 ========== */

static bool spi_registered[E_DRV_SPI_MAX];

/* ========== HAL 封装（static，加 DEBUG 追踪） ========== */

static bool hal_spi_init(void *base)
{
    return tf_hal_spi_init(base);
}

/* ========== 内部辅助 ========== */

static int spi_get_id(tf_drv_spi_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_spi_config_t *)h - spi_configs;
    if (id < 0 || id >= E_DRV_SPI_MAX || !spi_registered[id])
        return -1;
    return id;
}

/* ========== 对外函数实现 ========== */

tf_drv_spi_handle_t tf_drv_spi_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_SPI_MAX);
    if (id < 0 || id >= E_DRV_SPI_MAX) {
        return NULL;
    }

    if (spi_registered[id]) {
        return (tf_drv_spi_handle_t)&spi_configs[id];
    }

    const tf_drv_spi_config_t *cfg = &spi_configs[id];

    if (cfg->base == NULL) {
        return NULL;
    }


    /* ---- 配置 GPIO 引脚复用（委托 IO 子系统） ---- */
    if (cfg->io.mosi_id >= 0)
        tf_drv_io_register(cfg->io.mosi_id);
    if (cfg->io.miso_id >= 0)
        tf_drv_io_register(cfg->io.miso_id);
    if (cfg->io.sck_id >= 0)
        tf_drv_io_register(cfg->io.sck_id);
    if (cfg->io.cs_id >= 0)
        tf_drv_io_register(cfg->io.cs_id);

    /* ---- 初始化 SPI 外设寄存器（参数由 HAL 内部硬编码或回调） ---- */
    if (!hal_spi_init(cfg->base)) {
        return NULL;
    }

    spi_registered[id] = true;

    return (tf_drv_spi_handle_t)&spi_configs[id];
}

tf_drv_spi_handle_t tf_drv_spi_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_SPI_MAX) {
        return NULL;
    }

    if (!spi_registered[id]) {
        return NULL;
    }

    return (tf_drv_spi_handle_t)&spi_configs[id];
}

bool tf_drv_spi_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_SPI_MAX) ? spi_registered[id] : false;
    return ret;
}

void tf_drv_spi_tx(tf_drv_spi_handle_t h, const uint8_t *data, uint32_t len)
{
    int id = spi_get_id(h);
    if (id < 0) return;
    void *base = spi_configs[id].base;
    if (base) tf_hal_spi_tx(base, data, len);
}

void tf_drv_spi_rx(tf_drv_spi_handle_t h, uint8_t *buf, uint32_t len)
{
    int id = spi_get_id(h);
    if (id < 0) return;
    void *base = spi_configs[id].base;
    if (base) tf_hal_spi_rx(base, buf, len);
}

void tf_drv_spi_txrx(tf_drv_spi_handle_t h, const uint8_t *tx_data, uint8_t *rx_buf, uint32_t len)
{
    int id = spi_get_id(h);
    if (id < 0) return;
    void *base = spi_configs[id].base;
    if (base) tf_hal_spi_txrx(base, tx_data, rx_buf, len);
}
#endif /* MCU_CONTAIN_PRE_SPI */
