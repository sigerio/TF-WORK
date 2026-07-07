#ifndef TF_HAL_I2C_H
#define TF_HAL_I2C_H

#include <stdint.h>

/* ========== 初始化（各平台 HAL 内部完成参数配置） ========== */
void tf_hal_i2c_init(void *base);

/* ========== 主模式收发 ========== */
int tf_hal_i2c_master_tx(void *base, uint16_t addr, const uint8_t *data, uint32_t len);
int tf_hal_i2c_master_rx(void *base, uint16_t addr, uint8_t *buf, uint32_t len);
int tf_hal_i2c_master_tx_rx(void *base, uint16_t addr,
                            const uint8_t *tx_data, uint32_t tx_len,
                            uint8_t *rx_buf, uint32_t rx_len);

/* ========== 平台层 IRQ 接入 ========== */
void tf_hal_i2c_interrupt_enable(void *base, uint32_t int_mask);
void tf_hal_i2c_interrupt_disable(void *base, uint32_t int_mask);

#endif /* TF_HAL_I2C_H */
