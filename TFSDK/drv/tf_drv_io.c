/**
 * @file    tf_drv_io.c
 * @brief   IO 外设注册与原子操作实现
 *
 * 实现要点：
 *   - 用户必须提供 io_configs[] 数组（命名固定）
 *   - 所有硬件操作委托给 tf_hal_gpio.h 中的内联函数
 *   - 内部函数均为 static
 */

#include "tf_drv_io.h"
#include "../hal/tf_hal_gpio.h"
#include "../framework/tf_debug.h"
#include "../config/tf_board_config.h"

#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif

/* ========== 运行时状态 ========== */

static bool     io_registered[E_IO_MAX];
static uint8_t  io_current_state[E_IO_MAX];  /**< 当前电平状态（0/1），供 toggle 使用 */

/* ========== 内部辅助 ========== */

static int io_get_id(tf_drv_io_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_io_config_t *)h - io_configs;
    if (id < 0 || id >= E_IO_MAX || !io_registered[id])
        return -1;
    return id;
}

/* ========== HAL 封装（static，加 DEBUG 追踪） ========== */

static void hal_gpio_init(uintptr_t port, uint16_t pin, uint8_t mode, uint8_t pull, uint8_t speed, uint32_t alternate, uint8_t init_state)
{
    tf_hal_gpio_init(port, pin, mode, pull, speed, alternate, init_state);
}

static void hal_gpio_write(uintptr_t port, uint16_t pin, uint8_t level)
{
    tf_hal_gpio_write(port, pin, level);
}

static uint8_t hal_gpio_read(uintptr_t port, uint16_t pin)
{
    uint8_t val = tf_hal_gpio_read(port, pin);
    return val;
}

/* ========== 对外函数实现 ========== */

tf_drv_io_handle_t tf_drv_io_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_IO_MAX);
    if (id < 0 || id >= E_IO_MAX) {
        return NULL;
    }

    /* 防止重复注册 */
    if (io_registered[id]) {
        return (tf_drv_io_handle_t)&io_configs[id];
    }

    const tf_drv_io_config_t *cfg = &io_configs[id];


    /* 跟踪内部电平状态（供 toggle 使用） */
    io_current_state[id] = cfg->init_state;

    /* 初始化硬件 GPIO 模式 + 初始电平（HAL 内部处理输出写入）*/
    hal_gpio_init(cfg->port, cfg->pin, cfg->mode, cfg->pull,
                  cfg->speed, cfg->alternate, cfg->init_state);

    /* 所有初始化操作完成后再标记注册 */
    io_registered[id] = true;

    return (tf_drv_io_handle_t)&io_configs[id];
}

void tf_drv_io_set(tf_drv_io_handle_t h, uint8_t level)
{

    int id = io_get_id(h);
    if (id < 0) return;

    const tf_drv_io_config_t *cfg = &io_configs[id];


    hal_gpio_write(cfg->port, cfg->pin, level);
    io_current_state[id] = level;

}

uint8_t tf_drv_io_get(tf_drv_io_handle_t h)
{

    int id = io_get_id(h);
    if (id < 0) return 0;

    const tf_drv_io_config_t *cfg = &io_configs[id];
    uint8_t val = hal_gpio_read(cfg->port, cfg->pin);

    return val;
}

void tf_drv_io_toggle(tf_drv_io_handle_t h)
{

    int id = io_get_id(h);
    if (id < 0) return;


    /* 基于内部状态翻转，不重新读取硬件 */
    tf_drv_io_set(h, !io_current_state[id]);
}

tf_drv_io_handle_t tf_drv_io_get_handle(int id)
{

    if (id < 0 || id >= E_IO_MAX) {
        return NULL;
    }

    if (!io_registered[id]) {
        return NULL;
    }

    return (tf_drv_io_handle_t)&io_configs[id];
}

bool tf_drv_io_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_IO_MAX) ? io_registered[id] : false;
    return ret;
}
