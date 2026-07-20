#ifndef IEC104_APP_H
#define IEC104_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IEC104_APP_STATUS_POINT_COUNT  30U
#define IEC104_APP_MEASURE_POINT_COUNT 15U
#define IEC104_APP_TUNING_POINT_COUNT  13U

typedef struct {
    uint32_t ioa;
    uint8_t value;
    uint8_t quality;
} iec104_status_point_t;

typedef struct {
    uint32_t ioa;
    float value;
    uint8_t quality;
} iec104_measure_point_t;

void iec104_app_collect_status_points(iec104_status_point_t *points);
void iec104_app_collect_interrogation(iec104_status_point_t *status_points, iec104_measure_point_t *measure_points);

uint8_t iec104_app_remote_control_is_known(uint32_t ioa);
void iec104_app_remote_control_execute(uint32_t ioa);

int8_t iec104_app_tuning_index(uint32_t ioa);
uint8_t iec104_app_tuning_value_length(uint32_t ioa);
uint8_t iec104_app_tuning_value_parse(uint32_t ioa, const uint8_t *data, uint32_t *value, uint8_t *value_length, uint16_t *register_value);
uint8_t iec104_app_tuning_write(uint32_t ioa, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* IEC104_APP_H */
