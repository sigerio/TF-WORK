# USER LIB

Common user-level utility code shared by modules.

- `ringbuf.c/.h`: byte-oriented ring buffer for serial receive buffering.
  The internal state follows the RT-Thread style mirror-index design, using
  read/write indexes plus mirror bits to distinguish full and empty states.
- `modbus_rtu.c/.h`: Modbus RTU CRC, request builders, and response parsers
  for function codes 0x01, 0x03, 0x05, and 0x06.
