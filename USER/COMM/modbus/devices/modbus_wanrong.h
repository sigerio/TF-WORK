#ifndef MODBUS_WANRONG_H
#define MODBUS_WANRONG_H

#include "COMM/modbus/modbus_rtu.h"

/* 图片未给地址基准/功能码：第一轮暂按十进制 2000、0x06，需实机核对。 */
#define WANRONG_REG_COMMAND 2000U
typedef enum {
    WANRONG_STOP = 0,
    WANRONG_FORWARD = 1, /* 当前约定：整机推出 / 立杆升起 / 太阳板展开。 */
    WANRONG_REVERSE = 2, /* 对应反向动作；后续机构映射调整不改变线路命令值。 */
} wanrong_command_e;

uint32_t modbus_wanrong_build_command(uint8_t slave_addr, wanrong_command_e command, uint8_t *frame, uint32_t capacity);
modbus_rtu_status_t modbus_wanrong_parse_ack(const uint8_t *frame, uint32_t len, uint8_t slave_addr, uint16_t command);

#endif
