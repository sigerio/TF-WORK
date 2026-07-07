#ifndef MODULE_MODBUS_DEVICE_COMMON_H
#define MODULE_MODBUS_DEVICE_COMMON_H

#include <stdint.h>

#include "modbus_rtu.h"
#include "module_modbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XUNCHIP_ADDR_BROADCAST_QUERY        250U
#define XUNCHIP_REG_DATA0                   0x0000U
#define XUNCHIP_REG_DATA1                   0x0001U
#define XUNCHIP_REG_MODEL                   0x0064U
#define XUNCHIP_REG_POINT_COUNT             0x0065U
#define XUNCHIP_REG_DEVICE_ADDR             0x0066U
#define XUNCHIP_REG_COMM_MODE               0x0068U
#define XUNCHIP_REG_PROTOCOL                0x0069U
#define XUNCHIP_REG_CALIBRATION0            0x006BU

typedef struct {
    uint8_t addr;
    bool managed;
} module_modbus_device_config_t;

typedef union {
    uint32_t raw;
    struct {
        unsigned online     : 1;
        unsigned data_fault : 1;
        unsigned comm_fault : 1;
        unsigned reserved   : 29;
    } bit;
} module_modbus_device_status_t;

typedef enum {
    MODULE_MODBUS_REG_VALUE_U16 = 0,
    MODULE_MODBUS_REG_VALUE_I16,
} module_modbus_reg_value_type_t;

typedef struct {
    uint16_t reg_index;
    uint16_t value_offset;
    uint32_t mark_mask;
    uint32_t fault_mask;
    int32_t min_value;
    int32_t max_value;
    module_modbus_reg_value_type_t value_type;
} module_modbus_reg_range_item_t;

uint32_t module_modbus_build_read_regs(uint8_t addr, uint16_t start_reg,
                                       uint16_t count, uint8_t *out,
                                       uint32_t out_size);
uint32_t module_modbus_build_read_bits(uint8_t addr, uint16_t start_bit,
                                       uint16_t count, uint8_t *out,
                                       uint32_t out_size);

bool module_modbus_send_read_regs(uint8_t addr, uint16_t start_reg,
                                  uint16_t count);
bool module_modbus_send_read_bits(uint8_t addr, uint16_t start_bit,
                                  uint16_t count);

bool module_modbus_xunchip_read_measurements(uint8_t addr,
                                             module_modbus_device_type_t type);
bool module_modbus_xunchip_read_device_addr(uint8_t addr);

modbus_rtu_status_t module_modbus_parse_u16_regs(const uint8_t *frame,
                                                 uint32_t len, uint8_t addr,
                                                 uint16_t *regs,
                                                 uint16_t reg_capacity,
                                                 uint16_t *reg_count);
modbus_rtu_status_t module_modbus_parse_regs_with_ranges(
    const uint8_t *frame,
    uint32_t len,
    uint8_t addr,
    uint16_t expected_count,
    void *out,
    uint16_t status_offset,
    uint16_t marks_offset,
    uint16_t faults_offset,
    const module_modbus_reg_range_item_t *items,
    uint16_t item_count);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODBUS_DEVICE_COMMON_H */
