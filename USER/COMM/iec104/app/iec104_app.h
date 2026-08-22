#ifndef IEC104_APP_H
#define IEC104_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IEC104_APP_STATUS_POINT_COUNT  34U
#define IEC104_APP_MEASURE_POINT_COUNT 15U
#define IEC104_APP_TUNING_POINT_COUNT  13U

/* 单点遥信：信息对象地址、SPI 值和 SIQ 质量位。 */
typedef struct {
    uint32_t ioa;
    uint8_t value;
    uint8_t quality;
} iec104_status_point_t;

/* 短浮点遥测：信息对象地址、IEEE 754 值和 QDS 质量位。 */
typedef struct {
    uint32_t ioa;
    float value;
    uint8_t quality;
} iec104_measure_point_t;

/* 采集主动遥信或总召唤快照；输出数组长度由上面的点数宏固定。 */
void iec104_app_collect_status_points(iec104_status_point_t *points);
void iec104_app_collect_interrogation(iec104_status_point_t *status_points, iec104_measure_point_t *measure_points);

/* 遥控 IOA 到门机业务命令的映射。 */
uint8_t iec104_app_remote_control_is_known(uint32_t ioa);
void iec104_app_remote_control_execute(uint32_t ioa);

/* 遥调 IOA 的查表、值解析和空调参数写入。 */
int8_t iec104_app_tuning_index(uint32_t ioa);
uint8_t iec104_app_tuning_value_length(uint32_t ioa);
uint8_t iec104_app_tuning_value_parse(uint32_t ioa, const uint8_t *data, uint32_t *value, uint8_t *value_length, uint16_t *register_value);
uint8_t iec104_app_tuning_write(uint32_t ioa, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_APP_H */
