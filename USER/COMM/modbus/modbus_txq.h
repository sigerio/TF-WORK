#ifndef MODBUS_TXQ_H
#define MODBUS_TXQ_H

#include <stdbool.h>
#include <stdint.h>

#include "COMM/modbus/modbus_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Modbus 485 总线标识（每条总线对应一个 com_link 实例） */
typedef enum {
    MODBUS_BUS_WEATHER = 0,  /**< 气象板总线：xm7828/xm8189/xm8537/xm9505 */
    MODBUS_BUS_AC,           /**< 空调总线                                */
    MODBUS_BUS_COUNT,        /**< 总线数量                                */
} modbus_bus_t;

/** @brief 支持的 Modbus 从机设备类型 */
typedef enum {
    MODBUS_DEV_AC = 0,               /**< 空调设备                           */
    MODBUS_DEV_XM7828_TH,    /**< XM7828 温湿度传感器                */
    MODBUS_DEV_XM8189_WIND,      /**< XM8189 风速风向传感器              */
    MODBUS_DEV_XM8537_WATER,     /**< XM8537 水浸传感器                  */
    MODBUS_DEV_XM9505_RAIN,      /**< XM9505 雨量传感器                  */
    MODBUS_DEV_MACHINE_EXTEND,   /**< 整机推出：仅万融 4X-2000 命令 */
    MODBUS_DEV_POLE_RAISE,       /**< 立杆升起：仅万融 4X-2000 命令 */
    MODBUS_DEV_SOLAR_UNFOLD,     /**< 太阳板展开：仅万融 4X-2000 命令 */
    MODBUS_DEV_TIS,
    MODBUS_DEV_COUNT,            /**< 设备实例数量 */
} modbus_dev_type_t;

/** @brief 主站请求类型，描述请求/响应形态 */
typedef enum {
    MODBUS_REQ_READ_REGS = 0,    /**< 读寄存器响应：slave func byte_count data crc */
    MODBUS_REQ_READ_BITS,        /**< 读位响应：slave func byte_count data crc     */
    MODBUS_REQ_WRITE_COIL,       /**< 写单线圈响应：固定 8 字节回显              */
    MODBUS_REQ_WRITE_REG,        /**< 写单寄存器响应：固定 8 字节回显            */
    MODBUS_REQ_WRITE_REGS,       /**< 写多个寄存器：回显起始地址和数量 */
    MODBUS_REQ_COUNT,            /**< 请求类型数量                              */
} modbus_req_type_t;

/**
 * @brief 期望响应描述
 *        在发送一个 Modbus 请求帧之前，构造本结构描述期望从机返回的
 *        响应特征。当接收通道收到数据时，用此结构与接收帧的头部进行
 *        匹配（地址、功能码），匹配成功后才进行 CRC 校验和解析。
 */
typedef struct {
    modbus_bus_t bus;                /**< 该帧所属的 485 总线（路由依据）   */
    uint8_t slave_addr;                 /**< 从机 Modbus 地址                              */
    modbus_dev_type_t dev_type;      /**< 设备类型枚举，决定响应由哪个设备解析函数处理  */
    modbus_req_type_t req_type;      /**< 主站请求类型，决定响应帧形态和解析分发          */
    uint16_t start_addr;                /**< 起始寄存器地址或起始位地址                     */
    uint16_t quantity;                  /**< 期望读取的寄存器数量或位数量                   */
    uint8_t rsp_func;                   /**< 期望的 Modbus 功能码（如 0x03 读保持寄存器）   */
    uint8_t rsp_data_bytes;             /**< 期望响应中的数据字节数（不含地址、功能码、CRC）*/
    uint16_t echo_addr;                 /**< 写单线圈/寄存器响应中期望回显的地址             */
    uint16_t echo_value;                /**< 写单线圈/寄存器响应中期望回显的值               */
} modbus_rsp_expect_t;

/**
 * @brief 发送队列条目
 *        发送环形队列中的一个元素，记录待发帧在队列缓冲区中的位置
 *        以及该帧对应的响应期望（含所属总线）。
 */
typedef struct {
    uint16_t frame_pos;             /**< 帧数据在发送环形缓冲区中的起始索引       */
    uint16_t frame_len;             /**< 帧长度（字节）                          */
    modbus_rsp_expect_t expect;  /**< 该帧对应的期望响应描述（含 bus，出队时用）*/
} modbus_txq_item_t;

#if MODULE_MODBUS_ENABLE_DIAG
typedef struct {
    uint16_t write_pos;             /**< 当前环形缓冲区的写入位置索引              */
    uint32_t enqueue_count;         /**< 累计入队次数（自复位以来）               */
    uint32_t dequeue_count;         /**< 累计出队次数                             */
    uint32_t overwrite_drop_count;  /**< 因入队时覆盖旧数据而丢弃的旧帧计数       */
    uint32_t full_drop_count;       /**< 因队列已满而丢弃的旧帧计数               */
    uint32_t invalid_count;         /**< 无效入队请求（参数错误等）计数           */
} modbus_txq_diag_t;
#endif

/* 清空全部待发请求及其响应期望。 */
void modbus_txq_reset(void);
/* 复制一帧请求及其期望响应到共享发送队列。 */
bool modbus_txq_push(const uint8_t *frame, uint16_t frame_len, const modbus_rsp_expect_t *expect);
/* 仅取出指定总线的最早请求，避免不同总线互相占用发送机会。 */
bool modbus_txq_pop(modbus_bus_t bus, uint8_t *frame, uint16_t frame_capacity, uint16_t *frame_len, modbus_rsp_expect_t *expect);
/* 返回全部总线尚未发送的请求条目数。 */
uint8_t modbus_txq_count(void);
#if MODULE_MODBUS_ENABLE_DIAG
void modbus_txq_get_diag(modbus_txq_diag_t *diag);
void modbus_txq_clear_diag(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TXQ_H */
