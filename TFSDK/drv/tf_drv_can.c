/**
 * @file    tf_drv_can.c
 * @brief   CAN 总线外设配置与通信实现
 *
 * 实现要点：
 *   - 用户必须提供 can_configs[] 数组（命名固定）
 *   - 所有硬件操作委托给 tf_hal_can.h 中的内联函数
 *   - 内部函数均为 static
 *
 * ============ 初始化序列（GD32F10x 参考） ============
 *
 *   register(id)
 *   1. [PORT] 配置 GPIO 引脚（tf_drv_io_register）  — 先配引脚
 *   2. tf_hal_can_deinit(base)             — 复位 CAN 寄存器
 *   3. tf_hal_can_enter_init_mode(base)    — 进入初始化模式
 *   4. tf_hal_can_baudrate_config()        — 配置波特率（BT 寄存器）
 *   5. tf_hal_can_option_config()          — 配置选项（CTL 寄存器）
 *   6. tf_hal_can_leave_init_mode(base)    — 退出初始化模式 → 进入正常模式
 *
 *   filter_init(h, &filter, count)
 *   1. 遍历 filter 数组，逐个调用 tf_hal_can_filter_init()
 *     a. 禁用滤波器
 *     b. 配置 FDATA 寄存器（列表/掩码值）
 *     c. 配置 FMCFG（列表/掩码模式）
 *     d. 配置 FSCFG（16/32-bit 宽度）
 *     e. 配置 FAFIFO（关联 FIFO）
 *     f. 使能滤波器
 */

#include "tf_drv_can.h"
#include "tf_drv_io.h"
#include "../hal/tf_hal_can.h"
#include "../framework/tf_debug.h"
#if MCU_CONTAIN_PRE_FDCAN

/* ========== 内部断言 ========== */
#ifndef TF_ASSERT
    #include <assert.h>
    #define TF_ASSERT(x) assert(x)
#endif


#define TF_DRV_CAN_FILTER_LIMIT   2U

/* ========== 运行时状态 ========== */

static bool can_registered[E_DRV_CAN_MAX];

/* ========== HAL 封装（static，加 DEBUG 追踪） ========== */

static void hal_init_common(int id)
{
    const tf_drv_can_config_t *c = &can_configs[id];

    tf_hal_can_init(c->base);
}

/* ========== 内部辅助 ========== */

static int can_get_id(tf_drv_can_handle_t h)
{
    TF_ASSERT(h != NULL);
    if (h == NULL) return -1;
    int id = (tf_drv_can_config_t *)h - can_configs;
    if (id < 0 || id >= E_DRV_CAN_MAX || !can_registered[id])
        return -1;
    return id;
}

static void *can_get_base(tf_drv_can_handle_t h)
{
    int id = can_get_id(h);
    return (id >= 0) ? can_configs[id].base : NULL;
}

/* ========== 对外函数实现 ========== */

tf_drv_can_handle_t tf_drv_can_register(int id)
{

    TF_ASSERT(id >= 0 && id < E_DRV_CAN_MAX);
    if (id < 0 || id >= E_DRV_CAN_MAX) {
        return NULL;
    }

    if (can_registered[id]) {
        return (tf_drv_can_handle_t)&can_configs[id];
    }

    const tf_drv_can_config_t *cfg = &can_configs[id];

    if (cfg->base == NULL) {
        return NULL;
    }

             cfg->base, cfg->io.tx_id, cfg->io.rx_id);

    /* 配置 GPIO 引脚（GD32: 先配引脚，再配外设）*/
    if (cfg->io.tx_id >= 0)
        tf_drv_io_register(cfg->io.tx_id);
    if (cfg->io.rx_id >= 0)
        tf_drv_io_register(cfg->io.rx_id);

    /* 初始化 CAN 控制器寄存器 */
    hal_init_common(id);

    can_registered[id] = true;

    return (tf_drv_can_handle_t)&can_configs[id];
}

tf_drv_can_handle_t tf_drv_can_get_handle(int id)
{

    if (id < 0 || id >= E_DRV_CAN_MAX) {
        return NULL;
    }

    if (!can_registered[id]) {
        return NULL;
    }

    return (tf_drv_can_handle_t)&can_configs[id];
}

bool tf_drv_can_is_registered(int id)
{
    bool ret = (id >= 0 && id < E_DRV_CAN_MAX) ? can_registered[id] : false;
    return ret;
}

/* CAN data-path operations — message packing/unpacking + HAL dispatch */

tf_drv_can_mailbox_e tf_drv_can_message_transmit(tf_drv_can_handle_t h, const tf_drv_can_tx_message_t *msg)
{
    TF_ASSERT(h != NULL); if (h == NULL) return E_DRV_CAN_MAILBOX_NONE;
    TF_ASSERT(msg != NULL); if (msg == NULL) return E_DRV_CAN_MAILBOX_NONE;
    void *base = can_get_base(h);
    if (base == NULL) return E_DRV_CAN_MAILBOX_NONE;
    uint32_t tmi = (1U << 0);
    if (msg->tx_frame_type == E_DRV_CAN_FRAME_REMOTE) tmi |= (1U << 1);
    if (msg->tx_id_type == E_DRV_CAN_ID_EXTENDED) {
        tmi |= (1U << 2);
        tmi |= (msg->tx_efid & 0x1FFFFFFFU) << 3;
    } else {
        tmi |= (msg->tx_sfid & 0x000007FFU) << 21;
    }
    uint32_t tmp = (msg->tx_dlen & 0x0FU) << 0;
    uint32_t tmdata0 = 0, tmdata1 = 0;
    if (msg->tx_dlen > 0) tmdata0 |= (uint32_t)msg->tx_data[0] << 0;
    if (msg->tx_dlen > 1) tmdata0 |= (uint32_t)msg->tx_data[1] << 8;
    if (msg->tx_dlen > 2) tmdata0 |= (uint32_t)msg->tx_data[2] << 16;
    if (msg->tx_dlen > 3) tmdata0 |= (uint32_t)msg->tx_data[3] << 24;
    if (msg->tx_dlen > 4) tmdata1 |= (uint32_t)msg->tx_data[4] << 0;
    if (msg->tx_dlen > 5) tmdata1 |= (uint32_t)msg->tx_data[5] << 8;
    if (msg->tx_dlen > 6) tmdata1 |= (uint32_t)msg->tx_data[6] << 16;
    if (msg->tx_dlen > 7) tmdata1 |= (uint32_t)msg->tx_data[7] << 24;
    return (tf_drv_can_mailbox_e)tf_hal_can_message_transmit(base, tmi, tmp, tmdata0, tmdata1);
}

uint32_t tf_drv_can_transmit_states(tf_drv_can_handle_t h, tf_drv_can_mailbox_e mailbox)
{
    void *base = can_get_base(h);
    return base ? tf_hal_can_transmit_states(base, (uint8_t)mailbox) : 0;
}

void tf_drv_can_transmission_stop(tf_drv_can_handle_t h, tf_drv_can_mailbox_e mailbox)
{
    void *base = can_get_base(h);
    if (base) tf_hal_can_transmission_stop(base, (uint8_t)mailbox);
}

void tf_drv_can_message_receive(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo, tf_drv_can_rx_message_t *msg)
{
    TF_ASSERT(h != NULL); if (h == NULL) return;
    TF_ASSERT(msg != NULL); if (msg == NULL) return;
    void *base = can_get_base(h);
    if (base == NULL) return;
    uint32_t mi, mp, md0, md1;
    tf_hal_can_message_receive(base, (uint8_t)fifo, &mi, &mp, &md0, &md1);

    msg->rx_id_type = (tf_drv_can_id_type_e)((mi >> 2) & 0x01U);
    msg->rx_frame_type = (tf_drv_can_frame_type_e)((mi >> 1) & 0x01U);
    msg->rx_dlen = (uint8_t)(mp & 0x0FU);
    msg->rx_fi = (uint8_t)((mp >> 8) & 0xFFU);
    msg->rx_sfid = (mi >> 21) & 0x7FFU;
    msg->rx_efid = (mi >> 3) & 0x1FFFFFFFU;

    msg->rx_data[0] = (uint8_t)(md0 & 0xFFU);
    msg->rx_data[1] = (uint8_t)((md0 >> 8) & 0xFFU);
    msg->rx_data[2] = (uint8_t)((md0 >> 16) & 0xFFU);
    msg->rx_data[3] = (uint8_t)((md0 >> 24) & 0xFFU);
    msg->rx_data[4] = (uint8_t)(md1 & 0xFFU);
    msg->rx_data[5] = (uint8_t)((md1 >> 8) & 0xFFU);
    msg->rx_data[6] = (uint8_t)((md1 >> 16) & 0xFFU);
    msg->rx_data[7] = (uint8_t)((md1 >> 24) & 0xFFU);
}

void tf_drv_can_fifo_release(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo)
{
    void *base = can_get_base(h);
    if (base) tf_hal_can_fifo_release(base, (uint8_t)fifo);
}

uint8_t tf_drv_can_receive_message_length_get(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo)
{
    void *base = can_get_base(h);
    return base ? tf_hal_can_receive_message_length_get(base, (uint8_t)fifo) : 0;
}
#endif /* MCU_CONTAIN_PRE_FDCAN */
