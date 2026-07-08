#ifndef TF_485_DEVICE_COMMON_H
#define TF_485_DEVICE_COMMON_H

#include <stdint.h>

#include "modbus_rtu.h"
#include "tf_485_master.h"

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
 *    tf_485_dev_status_t  --  设备运行状态（在线、传感器故障）
 *    tf_485_reg_map_t --  寄存器范围映射表项
 *
 *  各具体设备的数据类型（如 xm7828_th_data_t）均直接保存设备地址，
 *  并嵌入 status 表达设备运行状态。
 * ─────────────────────────────────────────────────────────────────────
 */

/**
 * @brief 设备运行状态
 *        设备健康状态标志，由 tf_485_master 主任务在收到有效响应
 *        或超时/解析错误时自动更新。
 *
 * @note  此结构经 memset 清零后再设置对应位，因此 offline 和
 *        无故障是零值初始状态。
 */
typedef struct {
    bool online;        /**< true = 设备在线，最近一次通信成功       */
    bool sensor_fault;  /**< true = 传感器故障，由设备状态寄存器上报  */
} tf_485_dev_status_t;

/**
 * @brief 寄存器数据类型枚举
 *        指示寄存器值的有符号/无符号属性，用于范围校验。
 */
typedef enum {
    TF_485_REG_U16 = 0, /**< 无符号 16 位值 */
    TF_485_REG_I16,     /**< 有符号 16 位值 */
} tf_485_reg_value_t;

/**
 * @brief 寄存器范围映射表项
 *        将一段连续的 Modbus 寄存器映射到设备结构体中的某个字段。
 *        用于 tf_485_parse_ranges() 通用解析函数，
 *        避免为每个设备手写解析逻辑。
 *
 *  使用示例：将 reg_offset=0x0000 的 1 个寄存器映射到 out->temperature
 *            的偏移 field_offset 处，取值范围 -400~800（十分之一度）。
 */
typedef struct {
    uint16_t reg_offset;            /**< Modbus 寄存器地址（相对起始地址）       */
    uint16_t field_offset;          /**< 在目标结构体中的字节偏移量               */
    int32_t min_value;              /**< 有效最小值（用于范围校验）               */
    int32_t max_value;              /**< 有效最大值                               */
    tf_485_reg_value_t reg_type;    /**< 寄存器值类型（有符号/无符号）             */
} tf_485_reg_map_t;

modbus_rtu_status_t tf_485_parse_u16_regs(const uint8_t *frame,
                                          uint32_t len,
                                          uint8_t slave_addr,
                                          uint16_t *regs,
                                          uint16_t reg_capacity,
                                          uint16_t *reg_count);

modbus_rtu_status_t tf_485_parse_ranges(
    const uint8_t *frame,
    uint32_t len,
    uint8_t slave_addr,
    uint16_t expected_regs,
    void *out,
    uint16_t status_offset,
    const tf_485_reg_map_t *maps,
    uint16_t map_count);

#ifdef __cplusplus
}
#endif

#endif /* TF_485_DEVICE_COMMON_H */
