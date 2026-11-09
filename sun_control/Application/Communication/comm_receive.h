#ifndef COMM_RECEIVE_H
#define COMM_RECEIVE_H

#include "uart_transport.h"

void rx_frame_init(void);
void communication_rx_task(void *pvParameters);

#endif /* COMM_RECEIVE_H */
