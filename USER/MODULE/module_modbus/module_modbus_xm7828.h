#ifndef MODULE_MODBUS_XM7828_H
#define MODULE_MODBUS_XM7828_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM7828_TEMP_MIN_CENTI_C             (-3000)
#define XM7828_TEMP_MAX_CENTI_C             8500
#define XM7828_HUMIDITY_MAX_CENTI_RH        10000U
#define XM7828_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM7828 温湿度传感器数据
 *        解析后缓存的传感器测量值。
 *
 * @note  温度单位：百分之一摄氏度（centi-celsius）
 *        例：temperature_centi_c = 2567 表示 25.67°C
 *        湿度单位：百分之一相对湿度（centi-RH）
 *        例：humidity_centi_rh = 5320 表示 53.20%RH
 */
typedef struct {
    uint8_t addr;                            /**< 从机 Modbus 地址              */
    module_modbus_device_status_t status;    /**< 设备运行状态（在线、传感器故障）*/
    int16_t temperature_centi_c;             /**< 温度（百分之一 °C），有符号   */
    uint16_t humidity_centi_rh;              /**< 湿度（百分之一 %RH），无符号  */
} xm7828_th_data_t;

modbus_rtu_status_t module_modbus_parse_xm7828(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm7828_th_data_t *out);
bool module_modbus_xm7828_read_measurements(uint8_t addr, uint16_t start_reg,
                                            uint16_t reg_count);
bool module_modbus_get_xm7828_data(xm7828_th_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM7828_H */
