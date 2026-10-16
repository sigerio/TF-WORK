# Sun Control RS485 Bridge

GD32F103RC / FreeRTOS / Keil ARM Compiler 5 工程。作为第二块分发板，在上级主控与三台万融电机、一台 MT80 太阳能控制器之间双向转发 Modbus RTU 报文。

只接收主控地址 `0x11`、`0x12`、`0x13`、`0x14` 的完整有效帧，选定下行串口后，三路电机改为地址 `0x01`，MT80 改为地址 `0x0A`，并重算 CRC；电机口只接收地址 `0x01`、MT80 口只接收地址 `0x0A` 的完整有效帧，根据来源串口恢复上行地址并重算 CRC。功能码和数据区原样保留，正常及异常响应均可转发。

其他地址（包括广播 `0x00`、旧气象 `0x02～0x05`、故障通知 `0xB4`）整帧忽略，不入队、不应答、不转发、不打印 RTT 帧日志。无 B4 故障缓存与蜂鸣器告警逻辑。

| 上行地址 | 设备 | 串口 | TX / RX | 485 方向脚 | 波特率 | 收发 |
| --- | --- | --- | --- | --- | --- | --- |
| — | 上级主控 | USART1 | PA2 / PA3 | PA1 | 9600 | DMA |
| `0x11` | 整机推出电机 | USART2 | PB10 / PB11 | PB2 | 9600 | 中断接收、轮询发送 |
| `0x12` | 立杆升起电机 | UART3 | PC10 / PC11 | PA15 | 9600 | 中断接收、轮询发送 |
| `0x13` | 太阳板展开电机 | UART4 | PC12 / PD2 | PB13（当前代码，接线待核对） | 9600 | 中断接收、轮询发送 |
| `0x14` | MT80 控制器 | USART0 | PA9 / PA10 | PA8 | 115200 | DMA |

均为 8N1，485 方向脚高电平发送、低电平接收。4G 将来与 USART1 共用，当前仅连接主控，不包含 4G 切换逻辑。

## 目录职责

| 目录 | 职责 |
| --- | --- |
| `Application/Startup` | 程序入口与调度器启动 |
| `Application/Communication` | 原始收帧、白名单、消息队列、地址转换、分发与日志 |
| `Application/Runtime` | FreeRTOS 配置、初始化钩子、任务创建、运行灯及看门狗 |
| `Platform/Board` | 引脚、串口、DMA 资源配置及 GPIO 初始化 |
| `Platform/Communication` | 串口中断/DMA 收发、485 方向、通信定时器 |
| `Platform/Interrupts` | MCU 中断入口 |
| `Libraries` | 独立的 Modbus CRC 与环形字节缓存 |
| `ThirdParty` | GD32 厂商库、FreeRTOS、SEGGER RTT |
| `tests` | 主机端通信、DMA、板级配置及工程路径检查 |
| `docs` | 接线、通信行为与实现边界 |
| `MDK` | Keil 工程、调试选项与构建产物 |

地址常量在 `Application/Communication/comm_dispatch.h`，物理引脚及波特率在 `Platform/Board/board_config.c`。本板不主动轮询设备、不生成运动或参数写入命令、不解析业务寄存器；主控的分段、轮询和 200 ms 响应超时由主控负责。

PC7 运行灯低电平亮，每 500 ms 翻转。PC1 蜂鸣器初始化为低电平，保持静音。板级初始化后启动 FWDGT，由空闲任务喂狗，标称复位超时 2 秒；不单独检测仍允许空闲任务运行的通信任务故障。

RTT Terminal 0 输出 `[12345 ms] [UARTn RX/TX len=N] XX XX ...`，仅打印白名单接收帧及发送接口接受的有效帧。DMA TX 日志表示 DMA 已启动；时间为日志生成时的系统运行毫秒数。缓冲不足时整条日志跳过，不阻塞通信。

## 构建与检查

打开 `MDK/sun_control.uvprojx`，使用 ARM Compiler 5.06u7。现有 Keil 目标仍为 `WET`，当前输出基名为 `sun_control`，产物为 `MDK/Objects/sun_control.axf` 和 `MDK/Objects/sun_control.hex`。

```sh
python3 tests/test_sun_routing.py
python3 tests/test_usart_dma_idle.py
python3 tests/test_rs485_tx_setup.py
python3 tests/test_watchdog.py
python3 tests/test_led_buzzer.py
python3 tests/test_project_layout.py
```

接收时序、缓存限制与协议依据见 `docs/hardware_and_communication.md`。主机模拟检查不代替设备实机验证。
