#ifndef MODBUS_COMMON_H
#define MODBUS_COMMON_H

#include <stdint.h>

#include "COMM/modbus/modbus_rtu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XUNCHIP_REG_DATA0                   0x0000U
#define XUNCHIP_REG_DATA1                   0x0001U
#define XUNCHIP_REG_MODEL                   0x0064U
#define XUNCHIP_REG_POINT_COUNT             0x0065U
#define XUNCHIP_REG_DEVICE_ADDR             0x0066U
#define XUNCHIP_REG_BAUDRATE                0x0067U
#define XUNCHIP_REG_COMM_MODE               0x0068U
#define XUNCHIP_REG_PROTOCOL                0x0069U
#define XUNCHIP_REG_CALIBRATION0            0x006BU

/*
 * 设备通用类型
 * ─────────────────────────────────────────────────────────────────────
 *  本文件定义了所有 Modbus 从机设备公用的基础结构体：
 *
 *    modbus_dev_status_t  --  设备运行状态（在线、传感器故障）
 *    modbus_reg_map_t     --  寄存器范围映射表项
 *
 *  各具体设备的数据类型（如 xm8537_water_data_t）均直接保存设备地址，
 *  并嵌入 status 表达设备运行状态。
 * ─────────────────────────────────────────────────────────────────────
 */

/**
 * @brief 设备运行状态
 *        设备健康状态标志。协议解析更新传入数据，设备管理层同步通信结果。
 */
typedef struct {
    bool online;        /**< true = 设备在线，最近一次通信成功       */
    bool sensor_fault;  /**< true = 传感器故障，测量越界或设备故障位  */
} modbus_dev_status_t;

/** @brief 寄存器数据类型枚举（有符号/无符号属性，用于范围校验） */
typedef enum {
    MODBUS_REG_U16 = 0, /**< 无符号 16 位值 */
    MODBUS_REG_I16,     /**< 有符号 16 位值 */
} modbus_reg_value_t;

/**
 * @brief 寄存器范围映射表项
 *        将一段连续的 Modbus 寄存器映射到设备结构体中的某个字段，
 *        用于 modbus_parse_ranges() 通用解析函数。
 */
typedef struct {
    uint16_t reg_offset;            /**< Modbus 寄存器地址（相对起始地址）       */
    uint16_t field_offset;          /**< 在目标结构体中的字节偏移量               */
    int32_t min_value;              /**< 有效最小值（用于范围校验）               */
    int32_t max_value;              /**< 有效最大值                               */
    modbus_reg_value_t reg_type; /**< 寄存器值类型（有符号/无符号）             */
} modbus_reg_map_t;

/* 校验读寄存器响应，并以主机字节序输出寄存器数组。 */
modbus_rtu_status_t modbus_parse_u16_regs(const uint8_t *frame, uint32_t len,
    uint8_t slave_addr, uint16_t *regs, uint16_t reg_capacity, uint16_t *reg_count);

/* 按映射表校验并写入目标设备数据结构，同时更新其通信状态字段。 */
modbus_rtu_status_t modbus_parse_ranges(const uint8_t *frame, uint32_t len, uint8_t slave_addr,
    uint16_t expected_regs, void *out, uint16_t status_offset, const modbus_reg_map_t *maps, uint16_t map_count);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_COMMON_H */
