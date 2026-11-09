#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

/* 缓冲、任务周期、超时与从机地址；保留外部 -D 覆盖能力。 */
#ifndef MODULE_MODBUS_RX_BUF_SIZE
#define MODULE_MODBUS_RX_BUF_SIZE 256U    /**< DMA 接收缓冲区大小（字节）         */
#endif

#ifndef MODULE_MODBUS_RINGBUF_SIZE
#define MODULE_MODBUS_RINGBUF_SIZE 512U   /**< 接收环形 FIFO 大小（字节）          */
#endif

#ifndef MODULE_MODBUS_TX_BUF_SIZE
#define MODULE_MODBUS_TX_BUF_SIZE 256U    /**< DMA 发送缓冲区大小（字节）         */
#endif

#ifndef MODULE_MODBUS_SEND_BUF_SIZE
#define MODULE_MODBUS_SEND_BUF_SIZE 256U  /**< 发送队列环形缓冲区大小（字节）     */
#endif

#ifndef MODULE_MODBUS_SEND_QUEUE_MAX
#define MODULE_MODBUS_SEND_QUEUE_MAX 16U  /**< 发送队列最大条目数                  */
#endif

#ifndef MODULE_MODBUS_MASTER_PERIOD_MS
#define MODULE_MODBUS_MASTER_PERIOD_MS 10U   /**< master_task 执行周期（毫秒）     */
#endif

#ifndef MODULE_MODBUS_QUEUE_FILL_PERIOD_MS
#define MODULE_MODBUS_QUEUE_FILL_PERIOD_MS 150U /**< 设备轮询入队周期（毫秒）  */
#endif

#ifndef MODULE_MODBUS_ENABLE_DIAG
#define MODULE_MODBUS_ENABLE_DIAG 0U  /**< 诊断功能开关：1=启用，0=禁用           */
#endif

#ifndef MODULE_MODBUS_RESPONSE_TIMEOUT_MS
#define MODULE_MODBUS_RESPONSE_TIMEOUT_MS      200U
#endif

/* 主控上行逻辑地址；设备默认 0x01/4800 由 wether 下行口处理。 */
#ifndef MODULE_MODBUS_WEATHER_SENSOR_ADDR
#define MODULE_MODBUS_WEATHER_SENSOR_ADDR      2U
#endif

#ifndef MODULE_MODBUS_XM8537_ADDR
#define MODULE_MODBUS_XM8537_ADDR              4U  /**< 气象总线：水浸传感器 */
#endif

#ifndef MODULE_MODBUS_AC_ADDR
#define MODULE_MODBUS_AC_ADDR                  1U  /**< 空调总线：空调设备 */
#endif

#ifndef MODULE_MODBUS_WEATHER_ADDR
#define MODULE_MODBUS_WEATHER_ADDR             0xB4U /**< 气象总线：weather 板，单向同步整机故障。 */
#endif

/* USART0 上行逻辑站号；下游映射由分发板处理。 */
#ifndef MODULE_MODBUS_MACHINE_EXTEND_ADDR
#define MODULE_MODBUS_MACHINE_EXTEND_ADDR 0x11U
#endif
#ifndef MODULE_MODBUS_POLE_RAISE_ADDR
#define MODULE_MODBUS_POLE_RAISE_ADDR 0x12U
#endif
#ifndef MODULE_MODBUS_SOLAR_UNFOLD_ADDR
#define MODULE_MODBUS_SOLAR_UNFOLD_ADDR 0x13U
#endif
#ifndef MODULE_MODBUS_MT80_ADDR
#define MODULE_MODBUS_MT80_ADDR 0x14U
#endif

/* MT80 下游默认 0x0A/115200，sun_control 需将逻辑 0x14 映射为 0x0A。 */
/* 短帧上限，不修改公共 RTU 标准上限。 */
#define MODULE_MODBUS_MT80_MAX_REGS 16U

#endif /* MODBUS_CONFIG_H */
