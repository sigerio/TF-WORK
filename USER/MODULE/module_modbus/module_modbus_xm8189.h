#ifndef MODULE_MODBUS_XM8189_H
#define MODULE_MODBUS_XM8189_H

#include "module_modbus_device_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XM8189_SPEED_MAX_CENTI_MS           3000U
#define XM8189_DIRECTION_MAX_CENTI_DEG      36000U
#define XM8189_MEASUREMENT_REG_COUNT        2U

/**
 * @brief XM8189 风速风向传感器数据
 *        解析后缓存的传感器测量值。
 *
 * @note  速度单位：百分之一 m/s（centi-m/s）
 *        例：speed_centi = 1234 表示 12.34 m/s
 *        风向单位：百分之一度（centi-degree）
 *        例：direction_centi_deg = 4500 表示 45.00°
 */
typedef struct {
    uint8_t addr;                               /**< 从机 Modbus 地址              */
    module_modbus_device_status_t status;       /**< 设备运行状态（在线、传感器故障）*/
    uint16_t speed_centi;                       /**< 风速（百分之一 m/s），0~3000 */
    uint16_t direction_centi_deg;               /**< 风向（百分之一度），0~36000  */
} xm8189_wind_data_t;

modbus_rtu_status_t module_modbus_parse_xm8189(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm8189_wind_data_t *out);
bool module_modbus_xm8189_read(uint8_t addr, uint16_t start_reg,
                                            uint16_t reg_count);
bool module_modbus_get_xm8189_data(xm8189_wind_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_XM8189_H */
