/**
 * @file    tf_drv_can.h
 * @brief   CAN 总线外设配置与通信控制
 *
 * CAN（控制器局域网络）总线外设，支持 CAN2.0A/B 协议。
 * GD32F10x 处理器集成 2 个 CAN 控制器（CAN0 / CAN1），共享 28 个
 * 滤波器组，支持标准帧（11-bit ID）和扩展帧（29-bit ID）。
 *
 * ============ 使用流程 ============
 *
 * 1. 用户定义 tf_drv_can_id_e 枚举（含 E_CAN_NONE = -1 哨兵）
 * 2. 用户填写 tf_drv_can_config_t 配置数组（命名 can_configs）
 * 3. 在 can_configs[] 中静态填写 IO 和 filters[] 配置
 * 4. 调用 tf_drv_can_register() 完成依赖注册、控制器初始化和静态滤波器下发
 * 5. 发送报文：构造 tx_message → message_transmit() → 轮询 transmit_states()
 * 6. 接收报文：receive_message_length_get() → message_receive()
 *
 * 示例——Loopback 轮询模式：
 * @code
 *   typedef enum { E_CAN_NONE=-1, E_CAN_0, E_CAN_MAX } tf_drv_can_id_e;
 *
 *   can_configs[E_CAN_0] = {
 *       .base               = (void *)CAN0,
 *       .io = {
 *           .tx_id = E_IO_CAN0_TX,
 *           .rx_id = E_IO_CAN0_RX,
 *       },
 *       .working_mode       = CAN_MODE_LOOPBACK,
 *       .resync_jump_width  = CAN_BT_SJW_1TQ,
 *       .time_segment_1     = CAN_BT_BS1_5TQ,
 *       .time_segment_2     = CAN_BT_BS2_3TQ,
 *       .prescaler          = 48,
 *       .auto_retrans       = 0,
 *       .filter_count       = 1,
 *       .filters = {
 *           [0] = {
 *               .filter_number      = 0,
 *               .filter_mode        = CAN_FILTERMODE_MASK,
 *               .filter_bits        = CAN_FILTERBITS_32BIT,
 *               .filter_list_high   = 0x7AB << 5,
 *               .filter_list_low    = 0x0000,
 *               .filter_mask_high   = 0x7FF << 5,
 *               .filter_mask_low    = 0x0004,
 *               .filter_fifo_number = 1U,
 *           },
 *       },
 *   };
 *
 *   tf_drv_can_handle_t can_h = tf_drv_can_register(E_CAN_0);
 *
 *   // 发送
 *   tf_drv_can_tx_message_t tx = {
 *       .tx_sfid = 0x7AB,
 *       .tx_id_type = E_DRV_CAN_ID_STANDARD,
 *       .tx_frame_type = E_DRV_CAN_FRAME_DATA,
 *       .tx_dlen = 2,
 *       .tx_data = {0xAB, 0xCD},
 *   };
 *   tf_drv_can_mailbox_e mb = tf_drv_can_message_transmit(can_h, &tx);
 *
 *   // 接收
 *   while (tf_drv_can_receive_message_length_get(can_h, E_DRV_CAN_FIFO_1) == 0);
 *   tf_drv_can_rx_message_t rx;
 *   tf_drv_can_message_receive(can_h, E_DRV_CAN_FIFO_1, &rx);
 * @endcode
 */

#ifndef TF_DRV_CAN_H
#define TF_DRV_CAN_H

#include "tf_board_config.h"
#include <stdint.h>
#include <stdbool.h>
 




#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

typedef enum {
    E_DRV_CAN_ID_STANDARD = 0, /**< 标准帧（11-bit ID） */
    E_DRV_CAN_ID_EXTENDED = 1, /**< 扩展帧（29-bit ID） */
} tf_drv_can_id_type_e;

typedef enum {
    E_DRV_CAN_FRAME_DATA   = 0, /**< 数据帧 */
    E_DRV_CAN_FRAME_REMOTE = 1, /**< 远程帧 */
} tf_drv_can_frame_type_e;

typedef enum {
    E_DRV_CAN_FIFO_0 = 0, /**< 接收 FIFO0 */
    E_DRV_CAN_FIFO_1 = 1, /**< 接收 FIFO1 */
} tf_drv_can_fifo_e;

typedef enum {
    E_DRV_CAN_MAILBOX_0    = 0, /**< 发送邮箱 0 */
    E_DRV_CAN_MAILBOX_1    = 1, /**< 发送邮箱 1 */
    E_DRV_CAN_MAILBOX_2    = 2, /**< 发送邮箱 2 */
    E_DRV_CAN_MAILBOX_NONE = 3, /**< 无空邮箱 */
} tf_drv_can_mailbox_e;

/**
 * @brief CAN 引脚配置子结构体
 *
 * 引用 IO 枚举 ID，用户在 io_configs[] 中定义端口/引脚/模式。
 * register() 根据此配置自动完成 GPIO 初始化。
 */
typedef struct {
    int tx_id;              /**< IO 枚举 ID（如 E_IO_CAN_TX，-1=不使用） */
    int rx_id;              /**< IO 枚举 ID（如 E_IO_CAN_RX，-1=不使用） */
} tf_drv_can_io_cfg_t;

/**
 * @brief CAN 配置结构体——只保留资源描述字段
 *
 * 初始化参数（位时序/工作模式/滤波器等）由 HAL 层 per-instance 回调提供。
 */
typedef struct {
    void                 *base;                   /**< CAN 实例（如 CAN0 / CAN1） */
    tf_drv_can_io_cfg_t   io;                     /**< IO 依赖：TX/RX 引脚 ID */
} tf_drv_can_config_t;

#if MCU_CONTAIN_PRE_FDCAN
extern tf_drv_can_config_t can_configs[];
#endif

/**
 * @brief CAN 发送报文结构体
 */
typedef struct {
    uint32_t tx_sfid;                /**< 标准格式帧 ID（11-bit） */
    uint32_t tx_efid;                /**< 扩展格式帧 ID（29-bit） */
    tf_drv_can_id_type_e    tx_id_type;    /**< 标识符类型 */
    tf_drv_can_frame_type_e tx_frame_type; /**< 帧类型 */
    uint8_t  tx_dlen;                /**< 数据长度（0-8） */
    uint8_t  tx_data[8];             /**< 发送数据 */
} tf_drv_can_tx_message_t;

/**
 * @brief CAN 接收报文结构体
 */
typedef struct {
    uint32_t rx_sfid;                /**< 标准格式帧 ID */
    uint32_t rx_efid;                /**< 扩展格式帧 ID */
    tf_drv_can_id_type_e    rx_id_type;    /**< 标识符类型 */
    tf_drv_can_frame_type_e rx_frame_type; /**< 帧类型 */
    uint8_t  rx_dlen;                /**< 数据长度 */
    uint8_t  rx_data[8];             /**< 接收数据 */
    uint8_t  rx_fi;                  /**< 匹配的滤波器索引 */
} tf_drv_can_rx_message_t;

/**
 * @brief CAN 设备句柄（不透明指针）
 */
typedef void * tf_drv_can_handle_t;
tf_drv_can_mailbox_e tf_drv_can_message_transmit(tf_drv_can_handle_t h, const tf_drv_can_tx_message_t *msg);
uint32_t tf_drv_can_transmit_states(tf_drv_can_handle_t h, tf_drv_can_mailbox_e mailbox);
void tf_drv_can_message_receive(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo, tf_drv_can_rx_message_t *msg);
uint8_t tf_drv_can_receive_message_length_get(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo);
void tf_drv_can_transmission_stop(tf_drv_can_handle_t h, tf_drv_can_mailbox_e mailbox);
void tf_drv_can_fifo_release(tf_drv_can_handle_t h, tf_drv_can_fifo_e fifo);

/* ========== 对外接口 ========== */

/**
 * @brief 注册 CAN 设备
 *
 * 完成 CAN 控制器初始化：注册 TX/RX IO 依赖 → 复位寄存器 → 进入配置模式
 * → 配置波特率/选项 → 退出配置模式，并自动应用静态 filters[]。
 */
tf_drv_can_handle_t tf_drv_can_register(int id);

/**
 * @brief 获取句柄
 */
tf_drv_can_handle_t tf_drv_can_get_handle(int id);

/**
 * @brief 检查是否已注册
 */
bool tf_drv_can_is_registered(int id);

#ifdef __cplusplus
}
#endif


#endif /* TF_DRV_CAN_H */
