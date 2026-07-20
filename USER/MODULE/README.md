# USER MODULE

`USER/MODULE` is used for application-level driver interface modules.

Each concrete module should be placed in an independent directory, for example:

- `module_ws2812`
- `module_modbus`
- `iec104`

Keep module-local headers and source files inside the same directory unless
the interface is intentionally shared by multiple modules.
