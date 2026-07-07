#ifndef TF_HAL_SPI_H
#define TF_HAL_SPI_H

#include <stdint.h>
#include <stdbool.h>

bool tf_hal_spi_init(void *base);
void tf_hal_spi_tx(void *base, const uint8_t *data, uint32_t len);
void tf_hal_spi_rx(void *base, uint8_t *buf, uint32_t len);
void tf_hal_spi_txrx(void *base, const uint8_t *tx_data, uint8_t *rx_buf, uint32_t len);
uint32_t tf_hal_spi_stat_get(void *base);
uint8_t tf_hal_spi_data_read(void *base);
void tf_hal_spi_dma_tx_config(void *base, uint8_t enable);
void tf_hal_spi_dma_rx_config(void *base, uint8_t enable);
void tf_hal_spi_interrupt_enable(void *base, uint32_t int_mask);
void tf_hal_spi_interrupt_disable(void *base, uint32_t int_mask);

#endif /* TF_HAL_SPI_H */
