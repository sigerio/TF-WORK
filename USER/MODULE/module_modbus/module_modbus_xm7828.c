#include "module_modbus_xm7828.h"

#include <stddef.h>
#include <string.h>

#include "modbus_tx_queue.h"

#define XM7828_RANGE_ITEM(index_, field_, min_, max_, type_) \
    { (index_), (uint16_t)offsetof(xm7828_th_data_t, field_), \
      (min_), (max_), (type_) }

static const module_modbus_reg_range_item_t s_xm7828_ranges[] = {
    XM7828_RANGE_ITEM(0U, temperature_centi_c,
                      XM7828_TEMP_MIN_CENTI_C,
                      XM7828_TEMP_MAX_CENTI_C,
                      MODULE_MODBUS_REG_VALUE_I16),
    XM7828_RANGE_ITEM(1U, humidity_centi_rh,
                      0,
                      XM7828_HUMIDITY_MAX_CENTI_RH,
                      MODULE_MODBUS_REG_VALUE_U16),
};

bool module_modbus_xm7828_read_measurements(uint8_t addr, uint16_t start_reg,
                                            uint16_t reg_count)
{
    uint8_t frame[MODBUS_RTU_REQUEST_LEN];
    modbus_response_expect_t waiting;
    uint32_t len;

    if (addr == 0U) {
        return false;
    }

    len = modbus_rtu_build_read_request(addr,
                                        MODBUS_FC_READ_HOLDING_REGS,
                                        start_reg,
                                        reg_count,
                                        frame,
                                        sizeof(frame));
    if (len == 0U) {
        return false;
    }

    memset(&waiting, 0, sizeof(waiting));
    waiting.addr = addr;
    waiting.device = MODULE_MODBUS_DEV_XM7828_TH;
    waiting.function = MODULE_MODBUS_POLL_READ_REGS;
    waiting.start = start_reg;
    waiting.count = reg_count;
    waiting.expected_func = MODBUS_FC_READ_HOLDING_REGS;
    waiting.expected_bytes = (uint8_t)(reg_count * 2U);

    return modbus_tx_queue_enqueue(frame, (uint16_t)len, &waiting);
}

modbus_rtu_status_t module_modbus_parse_xm7828(const uint8_t *frame,
                                               uint32_t len, uint8_t addr,
                                               xm7828_th_data_t *out)
{
    return module_modbus_parse_regs_with_ranges(
        frame,
        len,
        addr,
        XM7828_MEASUREMENT_REG_COUNT,
        out,
        (uint16_t)offsetof(xm7828_th_data_t, status),
        s_xm7828_ranges,
        (uint16_t)(sizeof(s_xm7828_ranges) / sizeof(s_xm7828_ranges[0])));
}
