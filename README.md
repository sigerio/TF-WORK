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

- `comm_receive.c/.h`：接收原始帧、缓存、CRC 校验及标记入队，不修改报文地址。
- `comm_dispatch.c/.h`：设备模式配置、`node_consume_func[data_mark]` 分发、地址转换及发送重试。
- `comm_message.h`：消息标记、长度上限和队列节点；不再混入旧链表队列定义。
- `board_config.c/.h`：板级资源拓扑及 GPIO 初始化。
- `uart_transport.c/.h`：统一串口硬件接口；通过字节、错误和定时回调对接接收层，不包含应用消息头文件。
- `modbus_crc.c/.h`、`ringbuf.c/.h`：可独立复用的算法与数据结构。
- `freertos_hooks.c`：FreeRTOS 钩子及任务初始化。

## FreeRTOS 文件边界

MDK 使用的 FreeRTOS 内核文件位于 `ThirdParty/FreeRTOS/Kernel/`，许可证和版本历史位于 `ThirdParty/FreeRTOS/`。
本工程自己的配置与钩子位于 `Application/Runtime/`，不放入官方 FreeRTOS 目录。
选取范围以原 MDK 文件引用为准，保留其已引用的内核组件；没有更改 FreeRTOS 功能开关。

## 构建与检查

打开 `MDK/wether.uvprojx`，使用 ARM Compiler 5.06u7 编译。工程与调试选项均使用相对路径。

```sh
python3 tests/test_uart3_switch.py
python3 tests/test_project_layout.py
```

UART3 的雨量/水浸选择仍在 `Application/Communication/comm_dispatch.h` 中配置。接线、收帧限制及队列行为见 `docs/hardware_and_communication.md`。
