# Weather RS485 Bridge

GD32F103RC / FreeRTOS / Keil ARM Compiler 5 工程。主控与传感器之间转发 Modbus RTU 报文，队列管理任务按 `data_mark` 查函数指针数组完成处理和发送。

## 目录职责

| 目录 | 职责 |
| --- | --- |
| `Application/Startup` | 程序入口与调度器启动 |
| `Application/Communication` | 原始收帧校验、消息定义、地址转换、队列分发与任务 |
| `Platform/Board` | 板级引脚、串口和 DMA 资源配置、GPIO 初始化 |
| `Platform/Communication` | 集中的串口底层接口：RBNE、DMA、485 方向、发送、通信定时器 |
| `Platform/Interrupts` | MCU 中断入口 |
| `ThirdParty/GD32F10x` | GD32 官方 CMSIS、启动汇编及标准外设库，仅保留厂商文件 |
| `Libraries/RingBuffer` | 无项目、MCU 和 RTOS 依赖的环形字节缓存 |
| `Libraries/Modbus` | 无项目、MCU 和 RTOS 依赖的 Modbus CRC 计算与追加 |
| `ThirdParty/FreeRTOS/Kernel` | 从原始 FreeRTOS 中复制的 MDK 引用文件：内核、头文件、ARM_CM3/RVDS 移植、heap_4 |
| `Application/Runtime` | 本工程自己的 FreeRTOSConfig.h、钩子及通信任务创建 |
| `ThirdParty/SEGGER_RTT` | SEGGER RTT 官方调试支持 |
| `tests` | 主机端通信与工程路径检查 |
| `docs` | 硬件拓扑与通信行为说明 |
| `MDK` | Keil 工程、调试选项与构建产物 |

## 主要文件

- `comm_receive.c/.h`：接收原始帧、缓存、主控侧地址白名单、CRC 校验及标记入队，不修改报文地址。主控侧仅接收地址 `2、3、4、5、0xB4`，其他地址整帧丢弃，不打印、不分配内存、不入队；传感器侧地址 `1` 的回包正常接收。
- `comm_dispatch.c/.h`：传感器串口路由、`node_consume_func[data_mark]` 分发、地址转换及发送重试。
- 本机地址 `0xB4`：通过 USART1 接收主站 `B4 41 04 D0 D1 D2 D3 CRC低 CRC高` 故障通知，校验后由 `weather_fault_get()` 读取最新故障快照；单向通知，不应答、不转发。详细说明见 `docs/hardware_and_communication.md`。
- `comm_message.h`：消息标记、长度上限和队列节点；不再混入旧链表队列定义。
- `board_config.c/.h`：板级资源拓扑及 GPIO 初始化。
- `uart_transport.c/.h`：统一串口硬件接口；通过字节、错误和定时回调对接接收层，不包含应用消息头文件。
- `modbus_crc.c/.h`、`ringbuf.c/.h`：可独立复用的算法与数据结构。
- `freertos_hooks.c`：FreeRTOS 钩子及任务初始化。

板级初始化完成后，由软件启动独立看门狗 FWDGT；每次进入空闲任务钩子都立即喂狗，不阻塞空闲任务。复位超时按 IRC40K / 64、重装值 1249 配置，标称 2 秒，实际随内部 RC 时钟偏差变化。若空闲任务长期无法运行，硬件看门狗将复位；此机制不单独检测仍允许空闲任务运行的通信任务故障。

## FreeRTOS 文件边界

MDK 使用的 FreeRTOS 内核文件位于 `ThirdParty/FreeRTOS/Kernel/`，许可证和版本历史位于 `ThirdParty/FreeRTOS/`。
本工程自己的配置与钩子位于 `Application/Runtime/`，不放入官方 FreeRTOS 目录。
选取范围以原 MDK 文件引用为准，保留其已引用的内核组件；没有更改 FreeRTOS 功能开关。

## 构建与检查

RTT Terminal 0 使用 `SEGGER_RTT_printf` 按完整帧输出 `[12345 ms] [UARTn RX/TX len=N] XX XX ...`。
所有串口 RX/TX 仅打印长度为 4～256 字节且 Modbus CRC 校验通过的帧；短帧及 CRC 错误帧不打印。RX 在修改地址前打印，TX 在发送接口接受报文后打印，USART1 TX 表示 DMA 已启动。时间戳来自 FreeRTOS 系统 tick，换算为调度器启动后的毫秒数，表示日志生成时间；当前 1 kHz、32 位 tick 配置下约 49.7 天回绕。主控侧非白名单地址帧即使 CRC 正确也不打印；通过接收校验的 Modbus 异常响应仍可打印，不额外判断传感器数据语义。
日志覆盖五路串口：USART1 主控、USART2 三合一气象、UART3 独立水浸；USART0 原温湿度口与 UART4 原雨量逻辑路由闲置。USART1 为主控/4G 复用端口，当前仅启用主控通信，4G 暂不开发。
日志在任务上下文输出，发送忙或失败的重试不打印；坏帧不打印，也不会入队转发。RTT 空间不足时整条日志跳过，避免打印半条或阻塞通信；持续观察需要保持 RTT Viewer 读取。打印范围为接收缓存实际交给任务的数据，DMA 溢出或待取缓存占用造成的丢帧无法还原。

打开 `MDK/wether.uvprojx`，使用 ARM Compiler 5.06u7 编译。工程与调试选项均使用相对路径。

```sh
python3 tests/test_sensor_routing.py
python3 tests/test_usart1_dma_idle.py
python3 tests/test_rs485_tx_setup.py
python3 tests/test_watchdog.py
python3 tests/test_led_buzzer.py
python3 tests/test_project_layout.py
```

风速风向、雨量、温湿度统一通过 USART2 的三合一设备获取（4800/8N1，设备站号 0x01）；水浸改用 UART3（GD32 库命名，对应 USART3；9600/8N1，设备站号 0x01）。USART0 与 UART4 闲置，主控不再请求地址 5、3。接线、收帧限制及队列行为见 `docs/hardware_and_communication.md`。
