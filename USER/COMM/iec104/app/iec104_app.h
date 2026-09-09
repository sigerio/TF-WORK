#ifndef IEC104_APP_H
#define IEC104_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IEC104_APP_STATUS_POINT_COUNT  36U
#define IEC104_APP_MEASURE_POINT_COUNT 32U
#define IEC104_APP_TUNING_POINT_COUNT  17U

#define IEC104_IOA_YX_DOOR_LIMIT_FAULT     0x000017UL /* 门开/关限位冲突。 */
#define IEC104_IOA_YX_PLATFORM_LIMIT_FAULT 0x000018UL /* 平台伸出/收回限位冲突。 */

/* 单点遥信：信息对象地址、SPI 值和 SIQ 质量位。 */
typedef struct {
    uint32_t ioa;    /* 3 字节 IEC104 信息对象地址，取值范围 0x000000~0xFFFFFF。 */
    uint8_t value;   /* SPI 状态值，通常为 0 或 1。 */
    uint8_t quality; /* SIQ 品质位。 */
} iec104_status_point_t;

/* 短浮点遥测：信息对象地址、IEEE 754 值和 QDS 质量位。 */
typedef struct {
    uint32_t ioa;    /* 3 字节 IEC104 信息对象地址，取值范围 0x000000~0xFFFFFF。 */
    float value;     /* M_ME_NC_1 短浮点信息值。 */
    uint8_t quality; /* QDS 品质位。 */
} iec104_measure_point_t;

/* 采集主动遥信或总召唤快照；输出数组长度由上面的点数宏固定。 */
void iec104_app_collect_status_points(iec104_status_point_t *points);
void iec104_app_collect_interrogation(iec104_status_point_t *status_points, iec104_measure_point_t *measure_points);

/* 按本地点表查找/执行遥控 IOA；未知地址由上层返回 unknown IOA。 */
uint8_t iec104_app_remote_control_is_known(uint32_t ioa);
void iec104_app_remote_control_execute(uint32_t ioa);

/* 按本地点表查找遥调 IOA、解析对应值并写入空调或传感器参数。 */
int8_t iec104_app_tuning_index(uint32_t ioa);
uint8_t iec104_app_tuning_value_length(uint32_t ioa);
uint8_t iec104_app_tuning_value_parse(uint32_t ioa, const uint8_t *data, uint32_t *value, uint8_t *value_length, uint16_t *register_value);
uint8_t iec104_app_tuning_write(uint32_t ioa, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_APP_H */
