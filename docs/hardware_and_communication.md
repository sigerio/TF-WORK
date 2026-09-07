PC1-有源蜂鸣器控制-拉高有效
PB2-风向风速传感器485通信收发控制脚
PB10-usart2 tx -风向风速传感器485通信
PB11-usart2 rx -风向风速传感器485通信

PC11-usart3-rx 雨量传感器485-当前同时复用为水浸传感器485
PC10-usart3-tx 雨量传感器485-当前同时复用为水浸传感器485
PA15 -雨量传感器485通信收发控制脚

串口3当前由雨量或水浸二选一使用，不同时挂接两个默认地址为1的设备。
在 Application/Communication/comm_dispatch.h 中设置 UART3_DEVICE，或在 Keil 全局宏中定义：
- UART3_DEVICE=UART3_DEVICE_WATER：默认配置，主控地址4与从机地址1双向转换。
- UART3_DEVICE=UART3_DEVICE_RAIN：主控地址3与从机地址1双向转换。
未选设备的请求和响应不转发；两个模式均使用 PC10/PC11、PA15 和 9600 波特率。
BOARD_USART_RAIN 是现有串口3的物理通道标识，水浸模式也使用该通道。
串口3完成收帧后以 UART3_RX_MARK 标记原始消息并提交 main_queue。

通信收发流程（应用层位于 Application/Communication，串口底层位于 Platform/Communication/uart_transport.c）：
- 启动钩子创建 comm_rx 与 comm_queue 两个任务，初始化接收缓存后再使能串口。
- 从机串口0/2/3使用 RBNE 逐字节接收；每路独立组帧，完整原始报文进入 ringbuf，帧长度单独保存。
- TIMER2 作为通信专用定时器，1 MHz 自由计数，每100微秒检查帧间静默。当前验证配置为9600、8N1；断帧时间为3.5字符加 RX_TIMING_MARGIN_US（默认100微秒）。调整波特率时须复核检查周期与时序余量，尤其是高波特率。
- 每路从机缓存1024字节、最多8帧、单帧最多256字节。接收错误、帧内非法间隔、超长帧及缓存不足时丢弃整帧，待静默后恢复；channels[].dropped 可观察丢帧数。
- comm_rx 取出完整原始帧，只校验长度和CRC，复制原始字节并附加 DATA_FROM_* 标记；不解析业务、不改地址。CRC/长度无效累计到 invalid_frames。
- main_queue 容量20；接收层最多占用19个位置，为队首回调转换报文后重新入队预留1个位置，避免满队列死锁。
- comm_queue 保留 xQueuePeek 模式，按 node_consume_func[data_mark] 分发。回调返回true表示已处理或应丢弃，随后出队并释放报文；false表示暂时无法处理，保留队首并延时1毫秒重试。
- DATA_FROM_* 回调进行地址转换和CRC重算，重新提交 DATA_TO_*；DATA_TO_* 回调调用串口发送，不绕过统一分发表。
- 主控串口1仍用DMA接收，定时检查DMA进度并在静默后提交完整帧，支持256字节报文。当前保留1个待取帧，主控采用请求/响应方式；积压丢帧累计到 usart1_dma_rx_dropped。
- 主控DMA发送由USART TC中断完成收尾并切回485接收。发送忙或帧间隔未到返回false；从机发送等待也有超时，失败保留队首重试。
- UART4仍未使能，硬件改版后再增加独立水浸串口、来源映射和收帧通道。

主机检查：python3 tests/test_uart3_switch.py

PA10-usart0-rx-温湿度传感器485通信
PA9-usart0-rx-温湿度传感器485通信
PA8-usart0-rx-温湿度传感器485通信控制引脚

PC7-led控制脚-拉低有效
