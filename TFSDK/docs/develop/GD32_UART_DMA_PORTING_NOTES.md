# GD32 UART/DMA 开发注意事项

> 适用范围: `GD32F103RE/RC` 平台移植阶段
> 目标模块: `UART`、`DMA`
> 用途: 记录本次开发中已经验证过的注意事项，供后续其他硬件平台开发参考

---

## 1. 文档定位

本文档不是讨论记录，也不是临时方案草稿。

本文档只记录三类内容:

- 已经在本次 `GD32` 开发中确认过的实现注意事项
- 已经踩过的兼容性和分层问题
- 后续移植其他平台时可直接复用的检查项

若与 [ARCHITECTURE.md](../ARCHITECTURE.md) 冲突，以 `ARCHITECTURE.md` 为准。

---

## 2. UART 开发注意事项

### 2.1 先确认厂商外设命名差异

`GD32F103` 的外设命名与 `STM32` 常见叫法不同:

- `USART0` 对应常见语义里的 `USART1`
- `USART1` 对应常见语义里的 `USART2`
- `USART2` 对应常见语义里的 `USART3`

因此板级资源表必须先确认真实外设基地址，再写入 `uart_configs[]`。

本次已确认:

- `E_DRV_UART_0 -> USART0`
- `E_DRV_UART_1 -> USART1`
- `E_DRV_UART_2 -> GD32 USART2（对应常见语义 USART3）`

不要沿用旧平台残留的 `USART1/USART2` 习惯写法，否则 `board config` 会直接映射错外设。

### 2.2 `tf_board_config.h` 只描述当前工程资源，不描述 MCU 全量容量

`tf_board_config.h` 的职责是列出当前工程正在使用的资源枚举。

不应在这个文件中继续定义:

- `TF_UART_MAX`
- `TF_SPI_MAX`
- `TF_DRV_DMA_CH_MAX`
- `TF_DRV_TIMER_CH_MAX`
- 其他“MCU 最大容量”或“静态数量宏”

原因:

- 这些宏会把“当前工程资源”与“MCU 全量能力”混在一起
- 很容易把未使用资源数量错误上推到公共层
- 会让后续平台移植时误以为 `board_config` 在描述芯片完整资源图

正确做法是:

- `tf_board_config.h` 只保留当前资源枚举
- 驱动层数组尺寸直接基于当前资源枚举推导
- HAL 层若需要设备表或通道表，也应优先基于 `board config` 的当前资源枚举推导

不要这样做:

- 在 HAL 内重新定义一套与 `board config` 脱离的资源数量常量
- 在 HAL 内按“MCU 理论最大资源数”分配静态表，而不参考当前工程资源枚举

### 2.3 对寄存器型官方库，HAL 可以保持无状态

`GD32F10x Firmware Library` 本质上是围绕寄存器操作的官方库，不像 `STM32H5 Cube HAL` 那样强依赖 `UART_HandleTypeDef`。

因此 `GD32 UART HAL` 的实现原则应是:

- 不维护 `s_devs[]`
- 不做 `base_to_idx()`
- 不维护 HAL 层 `inited`
- 直接使用 `config.base` 对应的外设基地址

这样实现更简单，也更符合 `GD32` 官方库使用方式。

### 2.4 不要把运行时状态塞进 config

本次讨论中已经否定以下做法:

- 把 HAL 层 `inited` 标志放进 `config`
- 把 DRV 层 `registered` 标志放进 `config`

原因:

- `config` 的语义是静态资源描述，不是运行时状态容器
- 会混淆“资源信息”和“运行时进度”
- 会提高后续维护复杂度

后续其他平台开发时也应保持同一原则。

### 2.5 不要在 GD32 UART HAL 中重复做 base 过滤

对 `GD32` 这类直接面向寄存器/官方库的实现，不需要为了“防非法 base”再额外套一层 HAL 过滤。

原因:

- `base` 已经由 `board config + drv register` 提供
- HAL 再做一层 `base` 映射或白名单判断，收益低，复杂度高
- 若 `base` 非法，初始化阶段通常会直接暴露问题

注意:

- 这里说的是 `GD32 UART HAL` 的最小实现原则
- 不代表所有平台都必须去掉校验，是否保留取决于底层库模型

### 2.6 GPIO 与 UART 本体初始化要分层

本次实现里 `UART HAL` 只负责:

- 外设时钟使能
- UART 本体参数初始化
- 数据收发
- DMA 请求开关
- 中断开关

`GPIO` 初始化仍由 `IO` 子系统完成。

后续其他平台开发时不要把 GPIO 初始化重新塞回 `UART HAL`，否则会破坏既有分层。

### 2.7 波特率作为板级静态配置项

当前工程已将 `baudrate` 收敛进 `uart_configs[]`。

规则如下:

- `baudrate` 由板级资源表静态提供
- `tf_drv_uart_register()` 校验 `uart_configs[]` 后，应将整份 `uart_config` 透传给 HAL
- `GD32 UART HAL` 不再硬编码默认波特率
- `GD32 UART HAL` 在内部从 `cfg->base`、`cfg->baudrate` 读取所需字段
- `word length`、`stop bits`、`parity`、`mode` 仍由平台层固定

这样处理的原因是:

- 波特率属于串口实例的稳定板级属性
- 该属性直接依赖具体外设用途，放在 `board config` 中更直观
- 仍然不会把完整厂商初始化参数表上推到公共层
- DRV 不需要维护一套“拆字段再传参”的中间接口

### 2.8 UART 的 DMA 请求位和中断使能更适合作为内部接口

对 `GD32 UART HAL` 而言，以下动作更接近内部初始化步骤:

- `DMAT/DMAR` 请求位配置
- `RBNE/TBE/TC/IDLE` 等中断使能或禁能

这类操作的特点是:

- 一般在初始化或模式切换时配置一次
- 很少需要由外部在运行期反复直接调用
- 若提前暴露成公共 HAL 接口，会扩大公共面，但未形成稳定调用链

因此在没有明确 `DRV` 运行时需求前，更合适的做法是:

- 不放进公共 `tf_hal_uart.h`
- 留在平台实现内部使用
- 只有当上层确实形成稳定调用链时，再重新上升为公共接口

例外:

- 若某平台需要把 “UART 与 DMA 通道请求链路建立” 交给用户自己维护
- 则可以单独暴露一个平台相关接口，如 `uart_link_dma_ch`
- 但应在注释中明确要求：一旦启用 UART DMA，用户必须在完成 UART/DMA 初始化后显式调用该接口
- 这类接口的职责应限定为“建立请求链路”，不应顺带在 DMA HAL 中自动启用通道或启动传输

### 2.9 当前工程的调试打印口固定为 USART0

当前工程已明确：

- `USART0` 用于 `TF_PRINTF` / `TF_DEBUG` 输出
- `use_uart_dma()` 负责完成 `UART0`、`DMA0` 注册，建立 `USART0_TX -> DMA0_CH3` 请求链路，并在该入口内完成调试口启用后的首条输出
- `TF_PRINTF` / `TF_DEBUG` 的发送实现位于 `USER/uart_and_dma.c`，当前使用 `DMA0_CH3` 发送，并由发送路径轮询完成标志
- `TF_PRINTF` / `TF_DEBUG` 只有在 `tf_drv_uart_register(E_DRV_UART_0)`、`tf_drv_dma_register(E_DRV_DMA_0)` 完成后才允许对外输出
- `USART0` 初始化完成前的日志不回落到 semihosting / stdout

这样处理的原因是：

- 保持板端日志出口唯一，避免串口输出和 semihosting 混用
- 避免把调试口启用逻辑散落到 `main()` 或其他应用入口
- 保持 `USART0` 的用途稳定：既是板级串口资源，也是框架默认日志口
- 调试输出的数据搬运由 DMA 完成，CPU 只负责格式化、启动传输和处理中断收尾

---

## 3. DMA 开发注意事项

### 3.1 不要为 UART 单独再造一层“通用 DMA 绑定框架”

本次开发遵循的边界是:

- `UART HAL` 只控制 `DMAT/DMAR` 请求位
- `DMA DRV/HAL` 只负责 DMA 控制器和通道本身
- 不在公共层新增 `uart_dma_bind()` 之类的抽象

原因:

- `ARCHITECTURE.md` 已明确 SDK 不追求“通用 DMA 绑定框架”
- 各平台 DMA 绑定方式差异很大
- 强行统一只会提高接口复杂度

后续平台若有 DMA 需求，优先沿用本思路。

### 3.2 板级 DMA config 只写资源，不写完整策略

`dma_configs[]` 中只保留:

- `base`
- `channel_count`
- 各通道的 `channel`
- 各通道的 `periph_addr`
- 各通道的 `memory_addr`
- 各通道的 `direction`
- 各通道的 `enabled`

不要把以下内容塞进 `dma_configs[]`:

- `priority`
- `width`
- `increment mode`
- `circular mode`
- `DMA request 绑定链路`

这些都属于 HAL / Board Port 的平台初始化逻辑。

### 3.3 GD32 DMA HAL 可以采用“通道默认模板 + 运行时改地址/长度”的方式

本次 `GD32 DMA HAL` 的实现策略是:

- `tf_hal_dma_init()` 先基于通道默认模板初始化，再从 `dma_config` 读取 `periph_addr` / `memory_addr`
- `gd32_dma_default_init()` 应先于 `dma_deinit()` 执行，保持“先组装 init_struct，再清硬件寄存器，再调用 dma_init()` 的顺序
- 地址仍可由 `tf_hal_dma_addr_config()` 运行时覆盖
- 传输长度由 `tf_hal_dma_number_config()` 运行时设置

该策略适用于:

- 方向固定
- 数据宽度固定
- 平台映射清晰

不适用于:

- 同一通道要在多个方向之间动态复用
- 通道参数强依赖应用场景

因此后续平台开发前，应先判断 DMA 通道是否满足“固定模板”条件。

补充规则:

- `periph_addr`、`memory_addr` 的字段类型应与官方库保持一致
- 对 `GD32F10x`，即使用 `dma_parameter_struct` 和 `dma_*_address_config()` 所采用的 `uint32_t`
- `periph_addr` 可使用 `基址 + 偏移` 的常量表达式，避免 ARMCC5 对 `&USART_DATA()` / `&SPI_DATA()` 这类宏产生 extended constant initialiser 警告

### 3.4 DMA 通道映射必须以官方例程/手册为准

本次 `GD32F103` UART/SPI DMA 的映射依据来自官方例程与当前 WS2812 需求:

- `USART0_TX -> DMA0_CH3`
- `USART1_RX -> DMA0_CH5`
- `USART1_TX -> DMA0_CH6`
- `SPI0_TX -> DMA0_CH2`
- `SPI1_TX -> DMA0_CH4`
- `SPI2_TX -> DMA1_CH1`
- `GD32 USART2_TX / 常见语义 USART3_TX -> DMA1_CH4`
- `GD32 USART2_RX / 常见语义 USART3_RX -> DMA1_CH2`

注意:

- 不要凭 `STM32` 记忆直接套用
- 不要只看名字相似就假定通道一致
- 必须以官方例程或参考手册做最终确认

建议后续每次移植都执行以下检查:

1. 先查官方例程
2. 再查参考手册的 DMA request 映射
3. 最后写入 `board_dma.c`

### 3.5 DMA HAL 缺失时，先补 DMA 基础链路，再推进外设 DMA

本次推进 UART DMA 时发现:

- `gd32` 平台只有 UART 的 DMA 请求开关
- 但 `gd32` 平台没有对应的 `tf_hal_dma.c`
- `board_dma.c` 仍残留了 `stm32h5` 内容

因此正确顺序不是继续堆 UART 接口，而是先补齐:

1. `tf_hal_dma.c`
2. `tf_board_dma.c`
3. `MDK` 工程源文件纳入
4. 语法校验

这条顺序后续其他平台也可直接复用。

### 3.6 不要在 DMA HAL 中对整控制器全部通道做批量操作

若 DMA 通道已经由 `board config` 显式配置，则 HAL 不应再根据“DMA 控制器理论通道数”去批量遍历全部通道。

不要这样做:

- 在 HAL 内维护 `dma_channel_count()` 之类的控制器总通道数判断
- 对某个 DMA 控制器下的所有通道统一 `interrupt_enable/disable`

原因:

- HAL 不知道哪些通道真的属于当前工程资源
- 很容易误操作未在 `config` 中声明的通道
- 这种“整控制器广播式操作”本身就是危险行为

正确做法是:

- 只对 `config` 中已经声明的具体通道操作
- 若需要配置 DMA 中断，按通道使用 `interrupt_config()` 这类接口

---

## 4. 跨平台开发过程中的通用注意事项

### 4.1 不要让旧平台残留进入新平台资源表

本次发现过两类残留:

- `tf_board_uart.c` 中残留 `stm32h5xx.h`
- `tf_board_dma.c` 中残留 `GPDMA1`

这类问题风险很高，因为它们通常能通过“表面阅读”，但会在后续编译或运行阶段才暴露。

建议每次切换平台后优先检查:

- `board_*.c`
- `hal/<platform>/`
- 工程文件中的源文件路径

### 4.2 工程文件要同步纳入新增模块

仅修改源码还不够，`MDK` 工程文件也必须同步纳入新增文件。

本次已补入:

- `TFSDK/hal/gd32f10x/tf_hal_dma.c`
- `TFSDK/drv/tf_drv_dma.c`
- `TFSDK/config/tf_board_dma.c`

后续其他平台新增模块时，应把“工程文件同步更新”作为固定检查项。

### 4.3 先做语法校验，再做整工程联调

本次先使用 `arm-none-eabi-gcc -fsyntax-only` 对关键文件做快速校验，再继续推进。

建议每次开发至少检查:

- `hal` 实现文件
- `drv` 实现文件
- `board config` 文件

这样能在不依赖整工程链接的情况下，尽早暴露头文件依赖和语法问题。

### 4.4 注意 GD32 头文件与 `stdbool.h` 的兼容性

`GD32F10x` 的头文件中自定义了 `bool` 类型。

因此在 `gd32` 平台实现文件里，如果同时包含:

- `gd32f10x.h`
- `stdbool.h`

就可能产生类型冲突。

本次处理方式是:

- 优先包含 `gd32f10x.h`
- 再包含 SDK 自己的头文件

后续其他 `GD32` 平台开发时，建议把这条作为固定检查项。

---

## 5. 后续平台开发检查清单

开发新平台的 `UART/DMA` 时，至少检查以下项目:

1. `board_uart.c` 和 `board_dma.c` 是否还残留旧平台头文件或基地址
2. 板级资源表中的 `base` 是否和当前 MCU 的真实外设一致
3. `HAL` 是否真的需要内部状态表，还是可以做成无状态薄封装
4. 是否错误把运行时标志塞进了 `config`
5. 是否试图新增一套公共 `UART-DMA` 绑定接口
6. DMA request 映射是否已用官方例程或参考手册确认
7. 新增的 `hal/drv/config` 源文件是否已加入工程
8. 是否对关键文件做过独立语法校验

---

## 6. 本次 GD32 开发结论

本次 `GD32F103 UART/DMA` 开发已经收敛出以下稳定结论:

- `GD32 UART HAL` 采用无状态薄封装更合适
- `DMA` 保持独立父设备模型，不与 `UART` 做公共绑定抽象
- `config` 只保留静态资源描述，不承载运行时状态
- `HAL` 优先使用官方库 API，不无谓增加额外过滤层
- 平台移植时必须优先清理旧平台残留，再补齐本平台基础链路

这些结论后续可作为其他硬件平台开发时的默认起点。
