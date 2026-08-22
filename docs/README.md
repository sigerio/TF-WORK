# 工程结构与运行机制

本工程运行于 GD32F10x，控制门、停机坪、普通照明和 WS2812 灯效；同时通过
IEC 104 与上位机通讯，并以 Modbus RTU 轮询 RS-485 子设备。公开接口的职责和
状态字段含义写在各模块 `.h` 文件中；本文只说明模块关系和运行流程。

## 启动顺序

`Core/main.c` 按以下顺序初始化，所有硬件和模块初始化均在 OS 主循环开始前完成：

```text
tf_time_init
  -> debug_console_init
  -> iec104_port_init
  -> module_ws2812_init / light_output_init
  -> tf_modbus_master_init / tf_key_init
  -> module_moto_init / module_moto_sync_init
  -> iec104_server_init
  -> tf_os_init
  -> tf_usr_time_task_init / tf_usr_event_task_init
  -> tf_os_main_loop
```

`tf_os_main_loop()` 固定按“定时任务 -> 软事件 -> 队列任务”的顺序运行。中断只处理
外设事件和 DMA 字节搬运；协议解析、业务控制及状态上报均在主循环上下文执行。

## 周期任务

`USER/TASK/tf_usr_time_task.c` 注册各任务，默认周期如下。

| 周期 | 工作内容 |
|------|----------|
| 10 ms | `tf_modbus_master_task()`：RS-485 请求发送、响应解析与超时维护。 |
| 100 ms | `tf_modbus_query_task()`：填充下一轮 RS-485 巡检请求。 |
| 10 ms | `iec104_server_comm_task()`：IEC104 收帧、解析、发送和协议定时器。 |
| 100 ms | `iec104_server_report_task()`：检测变位并组装主动上送报文。 |
| 50 ms | `tf_key_scan_task()`：按键扫描、去抖和按键事件生成。 |
| 10 ms | `module_moto_sync_update()` 与 `module_moto_business_update()`：推进运动与业务状态。 |
| 50 ms | `module_ws2812_status_update()`：根据门/平台状态推进灯效。 |

软事件在 `USER/TASK/tf_usr_event_task.c` 注册。急停、按键和门/平台灯效状态由软事件
处理；RS-485 写操作也经软事件转入各设备的 Modbus 写接口，避免在 IEC104 解析链路中
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
  电机、灯光、WS2812 和 Modbus 子设备快照采集数据。

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
会话复位边界，详细报文和点表见 [IEC104 协议规范](iec104_protocol_spec.md)。

## RS-485 Modbus 数据流

`USER/COMM/modbus/` 是 Modbus 主站层，维护气象总线和空调总线。设备模块负责请求构造
和数据结构解析；`tf_modbus_txq` 保存待发帧以及预期响应；`tf_modbus_master` 按总线空闲
状态发送请求并分发响应。`com_link` 是共享 UART DMA 字节链路，只管理 DMA/FIFO/485
方向，不解释 Modbus 或 IEC104 帧。

```text
设备巡检或 IEC104 遥调
  -> Modbus 请求队列
  -> tf_modbus_master_task
  -> com_link TX DMA / RS-485
  -> 从机响应 DMA/FIFO
  -> 响应校验和设备数据快照更新
  -> IEC104 上送采集
```

## 门、平台与灯光控制链路

电机控制按三层划分，调用方向固定：

```text
按键或 IEC104 遥控
  -> motor_business：急停门控、短按切换策略
  -> motor_motion：门/平台目标状态和到位同步
  -> motor_driver：PWM 输出、到位反馈、限位和电吸锁
```

`motor_driver` 对每个电机管理两个方向输出。正常运动时只使能一个方向通道；停止时关闭
两个通道；刹车时使能两个通道。门关闭到位后锁定电吸锁，开门动作开始前先释放电吸锁。
`module_moto_run_status_t` 汇总门、平台、电机输出/反馈和限位快照，供 IEC104 点表和
调试器读取。

普通灯由 `light_output` 控制 PC3、PC4、PC5。WS2812 由 SPI/DMA 输出，由门和平台状态
驱动灯效；当前仅通过 IEC104 遥信上报灯效是否活动，不提供 WS2812 遥控点。

## 目录职责

| 目录 | 职责 |
|------|------|
| `Core/` | 启动入口、GD32 中断转发和芯片库配置。 |
| `USER/BOARD/` | 引脚、串口、DMA、SPI、PWM 等硬件资源拓扑。 |
| `USER/OS/` | 单线程软定时器、软事件和队列调度。 |
| `USER/TASK/` | 将工程模块注册到 OS 定时器和软事件。 |
| `USER/COMM/` | IEC104、Modbus 和 UART DMA 字节链路。 |
| `USER/CTRL/` | 电机、按键、普通灯和 WS2812 业务控制。 |
| `USER/LIB/` | 环形缓冲、Modbus RTU 编解码和内存管理基础组件。 |
