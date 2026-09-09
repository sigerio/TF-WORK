# 工程结构与运行机制

本工程运行于 GD32F10x，控制门、停机坪、普通照明、门加热解冻输出和 WS2812 灯效；同时通过
IEC 104 与上位机通讯，并以 Modbus RTU 轮询 RS-485 子设备。公开接口的职责和
状态字段含义写在各模块 `.h` 文件中；本文只说明模块关系和运行流程。

## 启动顺序

`Core/main.c` 按以下顺序初始化，所有硬件和模块初始化均在 OS 主循环开始前完成：

```text
board_time_init
  -> debug_console_init
  -> iec104_port_init
  -> module_ws2812_init / aux_output_init
  -> modbus_master_init / key_init
  -> motor_control_init
  -> iec104_server_init
  -> os_init
  -> usr_time_task_init / usr_event_task_init
  -> os_main_loop
```

`os_main_loop()` 固定按“定时任务 -> 软事件 -> 队列任务”的顺序运行。中断只处理
外设事件和 DMA 字节搬运；协议解析、业务控制及状态上报均在主循环上下文执行。

## 周期任务

`USER/APP/tasks/app_timer_task.c` 注册各任务，默认周期如下。

| 周期 | 工作内容 |
|------|----------|
| 10 ms | `modbus_master_task()`：RS-485 请求发送、响应解析与超时维护。 |
| 150 ms | `modbus_devices_poll()`：入队下一项 RS-485 巡检请求，单条请求响应超时为 100 ms。 |
| 10 ms | `iec104_server_comm_task()`：IEC104 收帧、解析、发送和协议定时器。 |
| 100 ms | `iec104_server_report_task()`：检测变位并组装主动上送报文。 |
| 50 ms | `key_scan_task()`：按键扫描、去抖和按键事件生成。 |
| 10 ms | 先读取物理急停并更新门控，再调用 `motor_control_update()`。 |
| 50 ms | `module_ws2812_status_update()`：根据门/平台状态推进灯效。 |

`usr_time_task_init()` 在按键、电机及 OS 初始化后先读取一次物理急停，再注册周期任务。
物理急停由运动任务直接检查，不等待 50 ms 按键扫描或软事件处理；释放后不自动恢复运动。

软事件在 `USER/APP/tasks/app_event_task.c` 注册。按键和门/平台灯效状态由软事件
处理，原急停/恢复事件入口保留；RS-485 写操作也经软事件转入各设备的 Modbus 写接口，避免在 IEC104 解析链路中
直接进行串口发送。

## IEC104 数据流

IEC104 模块位于 `USER/COMM/iec104/`，分层关系为：

```text
port -> iec104_server -> core -> app -> 控制模块 / Modbus 数据
```

- `port/` 负责串口 DMA 和接收 FIFO；一个串行数据块可以包含半帧或多个 APDU。
- `iec104_server` 是门面层。通信任务持续从 FIFO 取完整 APDU，半帧保留到下一周期；
  解析后的帧交给会话层，待发 APDU 再由该层取出并启动 DMA 发送。
- `core/` 包含 APCI/ASDU 编解码、会话序号窗口、确认、U 帧与定时器，以及 APDU 发送队列。
- `app/` 将 ASDU 绑定到本机点表：下行遥控进入门机或普通灯控制；总召唤和主动上送从
  电机、辅助输出（灯光、门加热解冻）、WS2812 和 Modbus 子设备快照采集数据。

接收链路如下：

```text
USART RX / DMA
  -> port 接收 FIFO
  -> iec104_server_comm_task
  -> APDU 解码
  -> iec104_session_handle_frame
  -> iec104_dispatch_handle
  -> iec104_app 的点表与业务动作
```

发送链路如下：

```text
主动变位或命令确认
  -> app / dispatch 组装 ASDU
  -> session 生成 I/S/U APDU 并入 txq
  -> iec104_server_comm_task
  -> port TX DMA
```

本工程使用串行字节流承载 IEC104 APDU。串行会话的 `STOPDT ACT` 是项目定义的逻辑
会话复位边界，详细报文和点表见 [IEC104 协议规范](../iec104_protocol_spec.md)。

## RS-485 Modbus 数据流

`USER/COMM/modbus/` 按设备业务、主站事务和设备协议分工。查看某个设备的协议时，
直接进入 `devices/modbus_<型号>.h/.c`；这些文件不访问队列、串口或全局缓存。

| 要查看的内容 | 文件与入口 |
| --- | --- |
| 轮询顺序、响应分发、在线状态与缓存 | `modbus_devices.c`：`modbus_devices_poll()`、`modbus_devices_parse_response()` |
| 业务异步写入、缓存读取 | `modbus_devices.h`：`modbus_*_queue_write*()`、`modbus_*_get*()` |
| 单条总线的发送、等待响应与超时 | `modbus_master.c`：`modbus_master_task()` |
| USART/DMA/GPIO 初始化与缓冲注册 | `modbus_port.c`：`modbus_port_open()` |
| 待发帧、总线路由与期望响应 | `modbus_txq.c/.h` |
| 设备组帧、寄存器含义与响应解析 | `devices/modbus_<型号>.c/.h`：`build_*()`、`parse_*()` |
| CRC、字节序和标准 RTU 帧格式 | `modbus_rtu.c/.h` |
| 从机地址、缓冲容量和任务时序 | `modbus_config.h` |

```text
轮询 / 业务写入（modbus_devices）
  -> 设备 build_*：校验参数、换算地址、生成请求帧
  -> modbus_txq：复制帧并保存路由、期望响应
  -> modbus_master：每条总线分别发送、等待、处理超时
  -> com_link：DMA/FIFO/485 方向控制
  -> modbus_master：取出匹配的响应
  -> modbus_devices：选择设备 parse_*，更新缓存与在线状态
  -> IEC104：通过 get 接口采集缓存
```

`build_*` 返回帧长，失败返回 0；`parse_*` 校验并更新调用方传入的数据。
`queue_write*` 返回 true 只表示请求已入队；实际通信结果在后续响应处理中更新。
气象设备 `get` 的返回值表示缓存复制成功，在线状态查看 `status.online`；空调整体
`get` 返回在线状态，参数和状态分段 `get` 返回对应有效标记。这些返回约定沿用现有行为。

轮询和响应分发继续使用函数指针表。每 150 ms 入队一项，顺序为温湿度 05、风速风向 02、
水浸 04、雨量 03、温湿度校准 05、风速风向校准 02，再读取空调的测量、湿度、控制和状态。
主站每 10 ms 执行一次，单条请求响应超时为 100 ms。两条总线独立等待响应，共享发送队列；
队列满时淘汰最早的旧请求，帧缓冲覆盖时淘汰被覆盖请求。

## 门、平台与辅助输出控制链路

电机控制分为两层：

```text
按键或 IEC104 遥控
  -> motor_control：门/平台命令、互锁顺序、换向等待、到位停机、急停和故障
  -> motor_driver：单台电机的正反向 PWM、停止和刹车
```

`motor_control` 通过板级接口读取四个限位并控制电吸锁，门的两台电机共用一组位置反馈。
关门前先收回平台，平台伸出前先开门；反向切换保留 100 ms 等待。门关到位后吸锁，开门前释放。
门与平台各自使用明确的内部状态机，定时任务仅调用一次 `motor_control_update()`。
`motor_control_status_t` 分别给出位置、实际动作和故障，状态读取仅复制快照；IEC104 不访问驱动方向。
`motor_driver` 对每台电机管理两个方向通道，运行时只使能一个，停止时全部关闭，刹车时同时使能。

实际到位判断使用光电限位开关，四个输入均为上拉输入、低电平有效：

| 引脚 | 到位含义 |
|------|----------|
| PB12 | 平台伸出到位 |
| PB13 | 平台收回到位 |
| PB14 | 门关闭到位，两台门电机共用 |
| PB15 | 门打开到位，两台门电机共用 |

启动阻止、运动停机、门/平台状态和反馈快照均使用这四个输入。旧 PA4/PA5、PC6～PC9
不再作为电机到位依据。目标到位后在运动更新中停止对应电机；门到位时两台门电机一起停止。

PB12/PB13 或 PB14/PB15 同时读低视为限位冲突。上电、每次运动更新（默认 10 ms，含空闲期）
及业务命令入口均检查；任一组冲突立即关闭平台和两台门电机输出，取消两轴运动及联动/换向等待。
限位恢复正常后自动清除对应故障，只刷新位置状态，必须收到新命令才可再次运动。
故障出现或清除时，运动周期任务立即触发现有 IEC104 变位上报；常规报告任务保留发送失败重试。
新增遥信 `0x000017` 为门限位冲突、`0x000018` 为平台限位冲突，1=故障、0=正常；
总召也包含这两个点，对应机构的原状态码在故障时为 3。

`aux_output` 控制 PC3 指示灯、PC4 内部照明和 PC5 门加热解冻伴热带。PC5 低电平导通，
上电默认关闭；软件通过 `aux_output_enable_door_heater()` / `aux_output_disable_door_heater()`
控制供电，用于门加热解冻；恒温由伴热带自身实现。WS2812 由 SPI/DMA 输出，由门和平台状态
驱动灯效；当前仅通过 IEC104 遥信上报灯效是否活动，不提供 WS2812 遥控点。
内部照明遥控为 `0x000222` 开启、`0x000223` 关闭；门加热遥控为 `0x000224` 使能、
`0x000225` 关闭，均沿用选择后执行流程。两路状态分别通过遥信 `0x000015`、`0x000016` 上报。

### WSE 灯效调试

本机灯带按 BRG（蓝、红、绿）顺序发送颜色字节，每字节高位先发；上层颜色接口仍使用 RGB。

USART2 处于 DEBUG 角色时，发送以下命令并以回车或换行结束。调试灯效覆盖整条 WS2812，
每次收到有效 WSE 命令都会重置播放进度，重复发送同一编号也会从头播放。

| 命令 | 灯效 |
| --- | --- |
| `WSE:0` | 退出调试覆盖，恢复当前门/平台业务灯效 |
| `WSE:1` | 反应堆心跳：冰蓝核心一强一弱双脉冲，向两端延迟扩散，深蓝待机，周期 3.6 秒 |
| `WSE:2` | 双转子涡轮：冰蓝、琥珀两条拖尾反向旋转，交会时叠加光强，每圈 3.2 秒 |
| `WSE:3` | 重载扫描：琥珀光晕与冷白扫描核平滑往返，两端各停留 600 ms，周期 4.8 秒 |
| `WSE:4` | 冲击涟漪：中央琥珀点火，冷白波向两端扩散，青色余波随后消退，周期 2.8 秒 |
| `WSE:5` | 低温等离子：深蓝底色上两层蓝青光带反向缓慢流动，分别以 7.2 秒、10.8 秒循环 |
| `WSE:6` | 跃迁蓄能：两端向中心蓄能 3 秒、锁定 400 ms、冷白释放 200 ms、冷却 1.4 秒、待机 1 秒 |

时序基于现有 50 ms 灯效任务。六种新效果使用冷白、冰蓝、深青和少量琥珀色，支持重复命令从头播放。
编号超出 0～6 或命令格式错误时保持当前灯效。业务输入在调试期间仍可更新，`WSE:0` 后重新参与灯效输出。
当前保留 12 颗灯珠配置；24 颗能让拖尾、扫描和涟漪过渡更细腻，但不是本组效果的必要条件。

[打开动态预览](ws2812_preview.html)：使用实际 C 灯效库输出的 24 秒帧序列，支持暂停、拖动时间和调速。
修改灯效后运行 `python3 tests/preview_ws2812.py` 更新预览；屏幕亮度和光晕仅作示意。

灯效库位于 `USER/CONTROL/ws2812/ws2812_effect.c/.h`，不依赖调试开关，也不负责 DMA 发送。
六种新效果分别对应 `ws2812_effect_reactor_heartbeat`、`ws2812_effect_dual_rotor`、`ws2812_effect_heavy_scan`、
`ws2812_effect_shockwave`、`ws2812_effect_cold_plasma`、`ws2812_effect_warp_charge`，参数为灯带编号和播放时间（ms）。
调用者负责绘制后提交；传入时间 0 可重新开始。调试层只选择效果、维护播放时间并提交整帧。

原六种调试效果通过 `ws2812_render_basic_effect(strip_id, effect, elapsed_ms)` 保留，枚举与效果对应如下。
现有常亮、闪烁、跑马、波浪、呼吸底层接口继续保留。

| 基础灯效枚举 | 保留的效果 |
| --- | --- |
| `WS2812_BASIC_WHITE_TRAILING` | 白色六颗拖尾，每 100 ms 前进一颗，头部亮度 80% |
| `WS2812_BASIC_GREEN_FLASH_THEN_STEADY` | 绿色亮 200 ms、灭 200 ms，三轮后常亮 |
| `WS2812_BASIC_YELLOW_STEADY` | 黄色常亮 |
| `WS2812_BASIC_RED_FLASH` | 红色亮 700 ms、灭 300 ms |
| `WS2812_BASIC_RED_STEADY` | 红色常亮 |
| `WS2812_BASIC_WHITE_BREATHING` | RGB 同步从 `0x000000` 渐亮至 `0xFFFFFF` 再渐暗，亮暗各 2 秒 |

## 目录职责

保留 `USER/` 根目录；顶层职责目录使用大写，子模块目录和文件名使用小写下划线。
头文件统一相对 `USER/` 引用，例如 `#include "CONTROL/motor/motor_control.h"`，
Keil 只需添加 `USER` 根目录作为用户代码头文件搜索路径。

```text
USER/
├── APP/
│   ├── app_events.h              # 应用事件编号
│   ├── app_status.c/.h           # 门、平台、通信在线状态位
│   └── tasks/                   # 定时任务与事件任务注册
├── BOARD/                       # 硬件资源表、SysTick 时间基准和延时
├── OS/                          # 定时器、事件、队列调度与内部接口
├── CONTROL/
│   ├── key/                     # 输入扫描、按键事件处理
│   ├── motor/                   # driver / motion / business
│   ├── output/                  # 指示灯、内部照明、门加热伴热带
│   └── ws2812/                  # driver / effect / status
├── COMM/
│   ├── link/                    # UART DMA / FIFO / RS-485 字节链路
│   ├── modbus/
│   │   └── devices/             # 空调、XM 系列设备协议与公共解析
│   └── iec104/
│       ├── core/                # 编解码、会话、发送队列
│       ├── app/                 # ASDU 分发、本机点表与业务绑定
│       └── port/                # 通用端口接口与 GD32 串口实现
├── COMMON/                      # 堆、环形缓冲、循环索引
└── DEBUG/                       # 调试串口、日志宏
```

`COMM/modbus/` 根目录保存设备管理、主站事务、硬件适配、队列和 RTU 编解码；具体设备协议位于
`devices/`。`Core/` 仍负责启动入口、中断转发和芯片库配置。

主要文件命名调整：

| 原文件 | 新位置 / 文件名 |
|--------|----------------|
| `tf_delay.c/.h` | `BOARD/board_time.c/.h` |
| `tf_debug.h` | `DEBUG/debug_log.h` |
| `tf_os*.c/.h` | `OS/os*.c/.h` |
| `tf_usr_time_task.c/.h` | `APP/tasks/app_timer_task.c/.h` |
| `tf_usr_event_task.c/.h` | `APP/tasks/app_event_task.c/.h` |
| `m_bit.c/.h` | `APP/app_status.c/.h` |
| `tf_tool.c/.h` | `COMMON/circular_index.c/.h` |
| `key.c/.h` | `CONTROL/key/key_input.c/.h` |
| `module_ws2812*.c/.h` | `CONTROL/ws2812/ws2812_driver、ws2812_effect、ws2812_status.c/.h` |
| `tf_modbus_*.c/.h` | `COMM/modbus/` 及 `devices/` 下的 `modbus_*.c/.h` |
| `iec104_port_usart1.c/.h` | `COMM/iec104/port/iec104_port_gd32.c/.h` |

函数、类型、变量和宏统一去除 `tf_` / `TF_` 项目前缀，定义、声明与调用同步更新。
时间接口使用 `board_time_*` / `board_delay_*` 命名，避免与内部计时变量和芯片库符号重名。
宏配置值、事件编号、初始化顺序和函数逻辑保持不变；日志中的函数名随重命名更新。GD32 端口文件
支持 USART1 / USART2 的既有角色选择，因此文件名不再限定为 USART1。

接口职责分为：

- `APP/app_events.h` 只定义应用事件编号；投递方不再依赖任务注册头文件。
- `APP/tasks/app_event_task.h` 声明事件任务注册入口。
- `OS/os_internal.h` 集中声明调度器内部初始化和处理入口，由 OS 实现文件共同引用；
  应用继续使用 `OS/os.h` 及各任务注册接口。
- `BOARD/board_time.h` 声明 SysTick 转发入口，`Core/gd32f10x_it.c` 通过头文件引用。

原空文件 `tf_usr_poll_task.c` 更名为 `APP/tasks/app_poll_task.c`，继续保持未加入构建。
Keil 工程中的源文件清单保持不变，并补齐用户头文件的工程显示项。
