/**
 * @file    tf_drv_uart.c
 * @brief   UART 驱动实现
 *
 * 实现要点：
 *   - 用户必须提供 uart_configs[] 数组（命名固定）
 *   - 数据操作调 HAL 原语，移植时替换 HAL 实现
 */
#include "tf_drv_uart.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_uart.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_USART

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif

/* ========== 运行时状态 ========== */
static bool uart_registered[E_DRV_UART_MAX];

/* ========== 内部辅助 ========== */

static int uart_get_id(tf_drv_uart_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_uart_config_t *)h - uart_configs;
    if (id < 0 || id >= E_DRV_UART_MAX || !uart_registered[id])
        return -1;
    return id;
}

/* ========== HAL 封装（static，加 DEBUG 追踪） ========== */

static void hal_uart_init(const tf_drv_uart_config_t *cfg)
{
    tf_hal_uart_init(cfg);
}

/* ========== 初始化 ========== */

tf_drv_uart_handle_t tf_drv_uart_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_UART_MAX);
    if (id < 0 || id >= E_DRV_UART_MAX) {
        return NULL;
    }

    if (uart_registered[id]) {
        return (tf_drv_uart_handle_t)&uart_configs[id];
    }

    const tf_drv_uart_config_t *cfg = &uart_configs[id];

    if (cfg->base == NULL || cfg->baudrate == 0U) {
        return NULL;
    }


    /* ---- 依赖注册：IO ---- */
    if (cfg->io.tx_id >= 0)
        tf_drv_io_register(cfg->io.tx_id);
    if (cfg->io.rx_id >= 0)
        tf_drv_io_register(cfg->io.rx_id);

    /* ---- 初始化 UART 外设寄存器 ---- */
    hal_uart_init(cfg);

    uart_registered[id] = true;

    return (tf_drv_uart_handle_t)&uart_configs[id];
}

tf_drv_uart_handle_t tf_drv_uart_get_handle(int id)
{
    if (id < 0 || id >= E_DRV_UART_MAX) {
        return NULL;
    }

    if (!uart_registered[id]) {
        return NULL;
    }

    return (tf_drv_uart_handle_t)&uart_configs[id];
}

bool tf_drv_uart_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_UART_MAX) ? uart_registered[id] : false;
    return ret;
}

/* ========== 数据收发 ========== */

void tf_drv_uart_tx(tf_drv_uart_handle_t h, const uint8_t *data, uint32_t len)
{
    int id = uart_get_id(h);
    if (id < 0) return;
    void *base = uart_configs[id].base;
    if (base) tf_hal_uart_tx(base, data, len);
}

void tf_drv_uart_rx(tf_drv_uart_handle_t h, uint8_t *buf, uint32_t len)
{
    int id = uart_get_id(h);
    if (id < 0) return;
    void *base = uart_configs[id].base;
    if (base) tf_hal_uart_rx(base, buf, len);
}
#endif /* MCU_CONTAIN_PRE_USART */
