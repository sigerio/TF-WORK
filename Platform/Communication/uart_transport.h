#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include "board_config.h"

#define UART_FRAME_MAX_SIZE 256U

#ifndef RX_TIMING_MARGIN_US
#define RX_TIMING_MARGIN_US 100U
#endif

void uart_transport_init(void);
uint16_t board_comm_time_us(void);
uint16_t board_rtu_gap_us(board_usart_id_e usart_id);
void board_comm_timer_irq_handler(void);
void board_usart_rx_irq_handler(board_usart_id_e usart_id);
void board_usart_rx_byte(board_usart_id_e usart_id, uint8_t byte);
void board_usart_main_dma_rx_init(void);
void board_usart_main_dma_rx_idle_irq_handler(void);
bool board_usart_main_dma_rx_fetch(uint8_t *data, uint16_t *data_len);
void board_usart_main_dma_tx_poll(void);
bool board_usart_main_dma_send(const uint8_t *data, uint16_t data_len);
bool board_usart_send(board_usart_id_e usart_id, const uint8_t *data, uint16_t data_len);

void board_usart_rx_error(board_usart_id_e usart_id);
void board_usart_rx_tick(uint16_t now);

#endif
