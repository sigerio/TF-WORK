#ifndef MODBUS_PORT_H
#define MODBUS_PORT_H

#include "COMM/link/com_link.h"
#include "COMM/modbus/modbus_txq.h"

/* 主站初始化时调用：配置 GPIO/USART/DMA/NVIC、分配静态缓冲并启动接收。 */
bool modbus_port_open(modbus_bus_t bus, com_link_t *link);

#endif /* MODBUS_PORT_H */
