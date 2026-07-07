# module_modbus

USART2 based 485 communication base module.

Current hardware uses an automatic-direction TTL-to-RS485 circuit, so this
module does not control DE/RE GPIO. MCU-side communication is plain USART2:

- RX: USART2 + DMA1_CH2 + IDLE interrupt
- TX: USART2 + DMA1_CH4, blocking until DMA FTF and USART TC

The module is now split by layer:

- `module_modbus.c/.h`: public facade, master scheduling glue, response
  dispatch, and device data cache access
- `modbus_rs485_bus.c/.h`: USART2 + DMA + RX ring buffer transport
- `modbus_tx_queue.c/.h`: queued TX frame storage and queue statistics
- `module_modbus_devices.c/.h`: common Modbus request helpers and aggregate
  includes for all supported device helpers
- `module_modbus_xm7828.c/.h`: XM7828 temperature/humidity device parser
- `module_modbus_xm8189.c/.h`: XM8189 wind device parser
- `module_modbus_xm8537.c/.h`: XM8537 water detector parser
- `module_modbus_xm9505.c/.h`: XM9505 rain device parser
- `module_modbus_ac.c/.h`: Modbus air conditioner protocol helpers
- `USER/LIB/modbus_rtu.c/.h`: generic Modbus RTU frame utilities

Master scheduling is split into two periodic tasks:

- 10 ms: consume the send queue, copy one queued frame into the DMA TX buffer,
  load the queued response context into `master.waiting_item`, start TX, match
  the response, handle timeout, and dispatch parsing.
- 100 ms: build polling frames and append them to the send queue.

Queued messages store the frame position in the module send buffer, frame
length, and the response context needed when that frame is sent. If the send
buffer tail does not have enough room for a new frame, writing wraps to the
buffer head and queued messages overlapped by the new frame are removed.
Frames are copied into the dedicated DMA TX buffer before transmission, so
later send-buffer overwrites cannot affect an active TX.

`master.waiting_item` is the only source of truth for the currently waiting
response. It is loaded only when a queued message is copied into the DMA TX
buffer for transmission; response matching, response length checks, parsing,
fault marking, and timeout handling read that loaded context.
