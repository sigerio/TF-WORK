# TF SDK 开发依据与开发规则

> 状态: 当前生效
> 作用: 本文件是 TF SDK 的开发依据、设计边界、实现规则和评审准则
> 优先级: 代码、注释、临时讨论、历史方案若与本文件冲突，以本文件为准

---

## 1. 文档定位

本文件不是设计讨论记录，也不是模块实现草稿集合。

本文件只承担四个职责:

- 定义 SDK 的定位
- 定义统一的分层和职责边界
- 定义公共接口、资源配置模型和注册流程规则
- 定义各模块后续开发时必须遵守的收敛方向

后续若产生新的模块讨论结果，应整理为可执行规则后再并入本文件，不保留“讨论过程式”表述。

---

## 2. SDK 定位

TF SDK 的定位是:

- 面向 GD32 单平台固件工程的外设注册框架
- 面向应用层的句柄管理和运行时访问框架
- 统一 `resource config + register + handle + runtime op` 使用路径
- 基于 `GD32F10x Firmware Library` 与必要寄存器访问的实现组织框架
- 当前目标是服务本工程的 GD32 板级实现，不再以 `STM32` 官方 `HAL` 兼容为目标

TF SDK 不再定义为:

- 平台无关的统一外设语义抽象层
- 一套重新包装厂商 SDK 全部参数的通用 HAL
- 一套为兼容 `STM32 Cube HAL` 或其他厂商库而预留的中间抽象层
- 一套完整的 config 驱动初始化框架
- 一套通用中断框架
- 一套通用时钟 / 复位资源管理框架
- 一套通用 DMA 绑定框架

一句话定义:

> TF SDK 统一的是外设的资源组织、注册入口、句柄管理和运行时访问路径，
> 不统一所有平台的初始化参数语义，也不尝试用 config 描述完整初始化过程。

### 2.1 平台边界

当前工程对平台边界作如下硬约束:

- 平台基线固定为 `GD32`，当前优先服务 `GD32F10x` 系列
- 文档、命名、初始化流程和问题分析以 `GD32F10x Firmware Library` 与 `GD32` 参考手册为准
- `STM32` 命名或历史迁移信息仅允许作为注释中的对照说明，不得反向塑造公共接口
- 新增能力时，优先在现有 `GD32` 实现上收敛，不为“未来可能支持别的平台”预埋抽象层
- 若 `GD32` 官方库无法满足需求，可直接操作寄存器，但应明确说明原因和影响范围

---

## 3. 总体分层

推荐分层如下:

```text
Application
    |
    v
DRV
    |
    v
HAL / Board Port
    |
    v
Vendor SDK / Registers

Board Resource Config
    └─ 提供静态资源描述
```

各层职责如下:

说明:

- 本工程中的 `HAL / Board Port` 是 `GD32` 平台实现层，不表示跨厂商兼容层
- 本工程中的 `Vendor SDK / Registers` 默认即 `GD32F10x Firmware Library` 与必要寄存器访问

- `Application`
  - 调用 `tf_drv_*_register(id)`
  - 使用句柄
  - 调用运行时接口
  - 不直接初始化硬件

- `DRV`
  - 定义轻量资源 config
  - 实现统一 `register()`
  - 管理注册状态和句柄
  - 同步注册必要依赖模块
  - 组织父模块内部子资源的同步初始化
  - 暴露必要运行时接口

- `HAL / Board Port`
  - 根据 `id` 和资源 config 手写平台初始化流程
  - 完成厂商 HAL handle 绑定
  - 完成外设本体初始化
  - 完成复杂平台流程，如 DMA link、IRQ/NVIC、EXTI、trigger、filter 等
  - 处理时钟、复位、平台绑定和底层细节

- `Vendor SDK / Registers`
  - 最终硬件访问入口

- `Board Resource Config`
  - 提供板级静态资源描述表
  - 不承载业务逻辑
  - 不承载完整初始化策略

---

## 4. 基本规则

### 4.1 用户使用模型

用户使用 SDK 的标准路径:

1. 填写必要的资源 config
2. 调用模块 `register()`
3. 通过句柄调用运行时接口

默认不采用以下模式:

- 用户先手工调用平台初始化函数，再调用 `tf_drv_*`
- 用户通过公共 config 描述完整厂商初始化参数
- 用户在公共 config 中直接管理 IRQ、NVIC、EXTI
- 用户在公共 config 中描述 DMA 与外设的绑定链路

说明:

- 初始化过程由 `register()` 触发
- 具体初始化动作由 HAL / Board Port 手写实现
- Application 不需要关心 GPIO、DMA、IRQ、clock、reset 的初始化顺序

### 4.2 公共接口形态

每个模块尽量统一为以下接口族:

- `tf_drv_xxx_register(id)`
- `tf_drv_xxx_get_handle(id)`
- `tf_drv_xxx_is_registered(id)`
- 必要的基础运行时接口

不追求统一的内容:

- 所有平台共享一套参数枚举
- 所有平台共享一套中断控制接口
- 所有平台共享一套 DMA 绑定接口
- 所有平台共享一套状态位 / 标志位模型
- 所有平台共享一套完整初始化 config

### 4.3 register() 标准流程

父模块 `tf_drv_*_register()` 应尽量遵循同一流水线:

1. 校验 `id`
2. 防止重复注册
3. 读取静态资源 config
4. 校验关键资源字段
5. 同步注册独立依赖模块，如 `IO`
6. 通过 HAL / Board Port 同步初始化外设本体和内部子资源
7. 保存句柄
8. 标记 `registered`
9. 返回句柄

规则说明:

- `register()` 是同步初始化边界
- 只有所有必要依赖注册和内部子资源初始化成功后，父模块才能标记为 `registered`
- 任一步失败，父模块 `register()` 返回失败，且不得标记为 `registered`
- SDK 不强制执行复杂回滚；已经注册成功的独立依赖模块可以保持已注册状态
- `register()` 是否同时意味着“启动”由模块自身定义，但必须在模块规则中单独明确
- DRV 调用 HAL 时，优先直接透传已校验的 `config`，避免在 DRV 层把字段拆散后再重组

---

## 5. 各层职责边界

### 5.1 Application

Application 允许:

- 调用 `tf_drv_*_register(id)`
- 使用 `tf_drv_*` 句柄
- 调用 `tf_drv_*` 运行时接口
- 组合业务逻辑

Application 禁止:

- 直接调用厂商 HAL 初始化函数作为业务前置
- 直接操作寄存器完成模块初始化
- 在业务代码中散落板级资源细节
- 手工注册父模块内部子资源

### 5.2 DRV

DRV 负责:

- 定义轻量资源 config
- 管理注册状态
- 管理句柄
- 同步注册独立依赖模块
- 组织父模块内部子资源的同步初始化
- 暴露必要公共运行时接口
- 将经过校验的静态 config 透传给 HAL / Board Port

DRV 不负责:

- 重新定义完整平台参数语义
- 将完整 TF 抽象初始化模型映射到厂商初始化结构体
- 管理 IRQ / NVIC / EXTI
- 统一时钟 / 复位调度
- 统一 DMA 与外设的绑定关系

### 5.3 HAL / Board Port

HAL / Board Port 负责:

- 根据 `id` 和资源 config 手写初始化流程
- 从 DRV 透传下来的 config 中读取并消费平台实际需要的字段
- 完成时钟使能、复位和官方库初始化步骤
- 处理平台专属流程和绑定关系
- 处理厂商 HAL handle 关联
- HAL 内部若需要设备表、通道表或容量边界，应优先基于 `board config` 中当前资源枚举推导
- 处理 DMA link、IRQ/NVIC、EXTI、trigger、filter 等复杂平台逻辑
- 优先使用官方库 API 完成硬件操作，非必要不直接操作寄存器

HAL / Board Port 不负责:

- 向 DRV 公共层暴露厂商初始化结构体类型
- 维护一套完整 TF 抽象值到厂商值的映射字典
- 在 HAL 内重新定义一套脱离 `board config` 的资源数量常量或“MCU 全量容量”常量

> 关于“非必要”的判断: 官方库 API 无法满足需求（如性能关键路径、官方库不提供的能力），
> 或官方库 API 引入的间接层会破坏模块职责边界时，允许直接操作寄存器，
> 但应在代码注释中说明原因。

### 5.4 Board Resource Config

Board Resource Config 负责:

- 提供各模块静态资源描述表
- 提供逻辑设备到具体硬件资源的映射
- 提供父模块到 IO / channel 等子资源的映射
- 只描述当前工程实际使用的资源枚举，不描述 MCU 持有的全量资源容量

Board Resource Config 不负责:

- 业务状态机
- 运行时动态操作
- 完整外设初始化策略
- 中断分发逻辑
- DMA 与外设的绑定链路

---

## 6. config 结构体规则

### 6.1 总原则

TF SDK 保留 config，但 config 的含义收敛为:

> 资源描述，不是完整初始化参数表。

config 允许描述:

- 逻辑设备对应的硬件基地址 `base`
- 逻辑设备依赖的 `io_id`
- 父外设内部硬件子资源 ID，如 timer channel、adc channel、dma channel
- 子资源是否启用

config 不描述:

- 完整厂商初始化参数
- 完整厂商初始化结构体
- IRQ / NVIC / EXTI
- DMA 与具体外设的数据通路绑定
- 时钟 / 复位策略
- 复杂工作模式、触发策略、滤波策略和中断策略

### 6.2 IO config 规则

`IO` 是唯一保留完整 config 驱动初始化的基础模块。

`IO config` 允许包含:

- port
- pin
- mode
- pull
- speed
- alternate function
- initial level
- active level

规则:

- `tf_drv_io_register()` 根据 `io_configs[]` 完成 GPIO 初始化
- `IO` 可被 Application 独立注册
- `IO` 也可被 UART / SPI / I2C / ADC / CAN / Timer 等父模块在 `register()` 内同步注册
- `IO config` 不包含 EXTI / NVIC / IRQ 分发策略

### 6.3 非 IO 外设 config 规则

除 `IO` 外，其他外设 config 只作为轻量资源描述。

允许的字段类型:

- `void *base`
- 经模块规则明确允许的少量板级静态参数，如 `UART baudrate`
- `io.*_id`
- `ch[].channel`
- `ch[].periph_addr`
- `ch[].memory_addr`
- `ch[].io_id`
- `ch[].enabled`
- 其他同级别的硬件资源 ID

不允许的字段类型:

- 厂商初始化结构体实例，如 `GPIO_InitTypeDef`、`UART_InitTypeDef`、`SPI_InitTypeDef`
- UART word length / stop bits / parity / mode
- SPI cpol / cpha / prescaler / frame size / endian
- I2C timing / addressing mode
- Timer prescaler / period / polarity / pulse / counter mode
- ADC sample time / trigger / continuous mode / DMA mode
- CAN bit timing / filter 细节 / message RAM 细节
- DMA width / priority / request / 外设绑定关系
- NVIC 优先级
- IRQ 使能策略
- ISR 回调函数
- EXTI 路由与边沿配置
- 平台侧中断清旗流程
- 通用时钟 / 复位调度参数

### 6.4 字段命名规则

推荐按资源类型组织字段:

- `base`
- `io.tx_id`
- `io.rx_id`
- `io.sck_id`
- `ch[i].channel`
- `ch[i].io_id`

推荐使用平台原生硬件资源宏:

- `USART1`
- `SPI1`
- `TIM_CHANNEL_1`
- `ADC_CHANNEL_4`

不要求为这些硬件资源重新定义一套 TF 抽象枚举。

---

## 7. 子模块与依赖关系规则

TF SDK 中的父子关系分为两类。

### 7.1 独立依赖模块

特征:

- 能独立存在
- 有自己的 config、register、handle
- 被父模块通过 ID 引用
- 父模块 `register()` 内同步调用其 `register()`

典型例子:

- `IO`

规则:

- `IO` 是当前默认允许同步注册的独立依赖模块
- 父模块依赖图必须单向，不允许依赖环
- 子模块不得反向注册父模块

### 7.2 父模块内部子资源

特征:

- 不独立暴露为外设
- 不单独持有公开句柄
- ID 保留在父模块 config 内
- 只在父模块 `register()` 内同步初始化

典型例子:

- `Timer channel`
- `ADC channel`
- `CAN filter bank/resource id`
- `DMA channel`

规则:

- 不提供 `tf_drv_timer_ch_register()`
- 不提供 `tf_drv_adc_ch_register()`
- 不提供 `tf_drv_can_filter_register()`
- 父模块负责在 `register()` 内通过 HAL / Board Port 完成内部子资源初始化

---

## 8. IRQ 规则

IRQ 相关内容不进入 TF SDK 公共 config。

统一规则如下:

- 中断源使能策略由 HAL / Board Port 处理
- NVIC 配置由 HAL / Board Port 处理
- EXTI 配置由 HAL / Board Port 处理
- ISR 绑定和回调分发由 HAL / Board Port 或用户平台代码处理

因此:

- DRV 不承诺通用 IRQ 抽象
- `register()` 只保证外设初始化完成
- `register()` 不保证中断链路已建立，除非该模块规则明确说明

HAL 层允许保留平台专属 IRQ / EXTI 辅助函数，但这些函数不构成 DRV 公共接口承诺，也不要求公共 config 增加对应字段。

---

## 9. 时钟与复位规则

时钟与复位属于 HAL / Board Port 初始化职责。

统一规则如下:

- DRV 不显式管理时钟
- DRV 不显式管理复位
- DRV 不提供统一时钟 / 复位调度框架
- HAL / Board Port 应按官方库推荐路径完成这些动作

这条规则的目的不是隐藏时钟，而是避免重新构建一套通用资源门控系统。

---

## 10. DMA 规则

`DMA` 可以作为父外设存在，也可以完全由 HAL / Board Port 手写管理。

若作为 TF SDK 父外设:

- `DMA config` 保留 `base`、必要 channel 资源 ID、`direction`，以及 `periph_addr` / `memory_addr` 这类地址字段
- `DMA channel` 是父模块内部子资源
- 不暴露独立 `dma_ch_register()`
- 不在公共 config 中描述 width / priority / request / 外设绑定关系

DMA 与 UART / SPI / I2C / ADC 等外设的数据通路绑定不属于 DRV 公共层职责:

- 各外设 config 不内嵌 `dma.*`
- 各外设 `register()` 不自动注册 DMA 依赖
- DMA link、request、HAL handle 绑定、IRQ 等由 HAL / Board Port 手写处理

### 10.1 ARMCC5 编译器注意事项

Keil ARMCC v5 对 `&(*(volatile uint32_t*)(addr))` 模式的静态初始化会报 `#1296-D: extended constant initialiser used` 警告，尽管生成的代码正确。

**影响位置**：`board_dma.c` 中 `dma_configs[].ch[].periph_addr` 的初始化，常见写法 `(uint32_t)&USART_DATA(USART0)` 会触发此警告。

**推荐规避方式**：在 board 文件中定义纯整数宏替代：

```c
/* USART 数据寄存器地址（DMA 外设地址）
 * 使用 (base + 0x04) 纯整数运算而非 &USART_DATA()，
 * 避免 ARMCC5 #1296-D 警告 */
#define USART0_DR_ADDR  ((uint32_t)(USART0 + 0x04U))
#define USART1_DR_ADDR  ((uint32_t)(USART1 + 0x04U))
#define USART2_DR_ADDR  ((uint32_t)(USART2 + 0x04U))
```

此宏展开为 `((uint32_t)(USART_BASE + 0xF400 + 0x04))` 形式的纯整数加法，ARMCC5 将其识别为合法常量初始化表达式。效果与 `(uint32_t)&USART_DATA(USART0)` 完全等价。

**原则**：使用符号表达式（`USARTx + 0x04U`）而非裸露数值（如 `0x40004404`）。

---

## 11. 模块开发规则

以下规则用于约束各模块的后续实现方向。

### 11.1 RCU

`RCU` 只保留以下职责:

- 系统时钟初始化入口
- 运行时频率查询

建议保留:

- `tf_drv_rcu_register()`
- `tf_drv_rcu_clock_freq_get()`
- 可选的 `get_handle()` / `is_registered()`

建议退出公共层:

- `periph_clock_enable`
- `periph_clock_disable`
- `periph_reset_enable`
- `periph_reset_disable`

规则:

- 外设 config 不扩展 `rcu_clock_id`
- 外设 `register()` 不显式调用 `tf_drv_rcu_periph_*`

实现约定（GD32F10x 平台）:

- 系统时钟硬件配置由 **CMSIS SystemInit()** 接管，通过 `system_gd32f10x.c` 的宏选择具体实现
- HAL 层 `tf_hal_system_clock_config()` **不重复实现硬件配置**，只负责将频率通知 `tf_delay` 模块
- CMSIS 已提供完整的 IRC8M PLL 配置函数（`system_clock_48m_irc8m` / `72m_irc8m` / `108m_irc8m`），对应宏分别为 `__SYSTEM_CLOCK_48M_PLL_IRC8M` / `__SYSTEM_CLOCK_72M_PLL_IRC8M` / `__SYSTEM_CLOCK_108M_PLL_IRC8M`
- 不使用外部晶振时必须在 `system_gd32f10x.c` 中**关闭** `__SYSTEM_CLOCK_xxx_PLL_HXTAL` 系列宏，**启用** `__SYSTEM_CLOCK_xxx_PLL_IRC8M` 系列宏，否则 `SystemInit()` 在等待 HXTAL 稳定时会卡死
- `tf_sysclk_freq_set()` 使用**硬编码值**（如 `108000000UL`）而非引用 CMSIS 宏，因为 `__SYSTEM_CLOCK_xxx` 宏定义在 `.c` 文件内部，其他翻译单元不可见
- `rcu_clock_freq_get()` 委托 GD32 固件库读取寄存器实时计算，用于运行时频率查询

### 11.2 IO

`IO` 是独立依赖模块。

负责:

- GPIO 初始化
- 初始电平写入
- 逻辑电平读写
- active level 管理

不负责:

- EXTI
- NVIC
- 通用 GPIO 中断抽象

规则:

- `tf_drv_io_register()` 初始化的是 GPIO，不是 GPIO+EXTI
- 其他父模块可以在 `register()` 内同步调用 `tf_drv_io_register(io_id)`

### 11.3 UART

`UART config` 只保留资源描述:

- `base`
- `baudrate`
- `io.tx_id`
- `io.rx_id`

规则:

- `register()` 内同步注册 TX/RX IO
- `baudrate` 作为板级静态配置项由 `uart_configs[]` 提供
- DRV 层将 `uart_config` 整体透传给 HAL，由 HAL 内读取 `base`、`baudrate`
- HAL / Board Port 手写 UART 其余初始化参数，如 word length、stop bits、parity、mode
- UART 与 DMA 的绑定不属于 DRV 公共层职责
- 当前工程的 `TF_PRINTF` / `TF_DEBUG` 默认通过 `UART_0 -> USART0` 输出
- `USART0` 日志发送实现放在 `USER/uart_and_dma.c`，当前固定使用 `DMA0_CH3` 并轮询发送完成标志
- 只有在 `tf_drv_uart_register(E_DRV_UART_0)`、`tf_drv_dma_register(E_DRV_DMA_0)` 以及 `UART0_TX DMA request` 建立完成后，`TF_PRINTF` / `TF_DEBUG` 才允许对外输出

### 11.4 SPI

`SPI config` 只保留资源描述:

- `base`
- `io.mosi_id`
- `io.miso_id`
- `io.sck_id`
- `io.cs_id`

规则:

- `register()` 内同步注册 SPI 相关 IO
- HAL / Board Port 手写 SPI 初始化参数，如 mode、cpol、cpha、prescaler、frame size
- SPI 与 DMA 的绑定不属于 DRV 公共层职责

### 11.5 I2C

`I2C config` 只保留资源描述:

- `base`
- `io.scl_id`
- `io.sda_id`

规则:

- `register()` 内同步注册 SCL/SDA IO
- HAL / Board Port 手写 I2C 初始化参数，如 timing、addressing mode
- I2C 与 DMA 的绑定不属于 DRV 公共层职责

### 11.6 ADC

`ADC` 是父外设。

`ADC config` 只保留资源描述:

- `base`
- `ch[].enabled`
- `ch[].channel`
- `ch[].io_id`

规则:

- 不引入 `adc_ch_register()` 公开接口
- `ADC channel` 不是运行时子模块
- 外部模拟输入通过 `ch[].io_id` 声明 IO 依赖
- `register()` 内同步注册 channel 对应 IO
- HAL / Board Port 手写 ADC 初始化参数，如 resolution、sample time、trigger、regular/injected、DMA
- ADC 与 DMA 的绑定不属于 DRV 公共层职责

### 11.7 CAN

`CAN` 是父外设。

`CAN config` 只保留资源描述:

- `base`
- `io.tx_id`
- `io.rx_id`
- 必要时保留 filter bank/resource id

规则:

- `register()` 内同步注册 TX/RX IO
- filter 不作为公开子模块
- HAL / Board Port 手写位时序、工作模式、filter 细节和平台专属配置
- 公共模型不围绕 bxCAN / FDCAN 专属细节组织
- 当前阶段不考虑 `CAN + DMA`

### 11.8 FWDGT

`FWDGT` 是极简父外设。

规则:

- 没有 `io` 依赖
- 没有 `dma` 依赖
- 没有公开子模块
- `register()` 即启动
- 运行时公共主接口只保留 `feed()`
- prescaler、reload、timeout 等初始化策略由 HAL / Board Port 手写，或由专门运行时 API 显式传入

### 11.9 Timer

`Timer` 是父外设。

`Timer config` 只保留资源描述:

- `base`
- `ch[].enabled`
- `ch[].channel`
- `ch[].io_id`

适用于:

- `timer + pwm ch`
- `timer + capture ch`

不适用于:

- `timer + dma` 公共绑定

规则:

- `timer` 是父外设
- `ch` 是内部硬件子资源
- `ch` 不对用户单独暴露注册接口
- `register()` 内同步注册 channel 对应 IO
- HAL / Board Port 手写 timer base、PWM、capture 等初始化参数
- `register()` 只初始化，不自动启动计数器，不自动建立中断链路

### 11.10 RTC

`RTC` 是父外设。

`RTC config` 只保留资源描述:

- `base`，若平台需要

规则:

- RTC 不依赖 IO
- RTC 不依赖 DMA
- backup domain、LSE/LSI、PWR、BDCR 等平台流程由 HAL / Board Port 处理
- alarm / wakeup 的 IRQ 链路不进入公共 config
- `register()` 只完成 RTC 初始化，不保证 alarm 中断链路已建立

---

## 12. Board Resource Config 规则

Board Resource Config 是 SDK 资源来源，不是业务代码容器。

推荐承载内容:

- `io_configs[]`
- `uart_configs[]`
- `spi_configs[]`
- `i2c_configs[]`
- `adc_configs[]`
- `can_configs[]`
- `timer_configs[]`
- `dma_configs[]`
- `rtc_configs[]`
- `fwdgt_configs[]`，若模块需要

其中:

- `io_configs[]` 是完整 GPIO 初始化资源表
- `uart_configs[]` 保留 `base`、`baudrate`、`io_id`
- `dma_configs[]` 保留 `base`、`channel id`、`periph_addr`、`memory_addr`、`direction`
- 其他 `xxx_configs[]` 只保留 `base`、`io_id`、`channel id` 等资源描述

推荐目录方向:

```text
tf_sdk/board/
    board_io.c
    board_uart.c
    board_spi.c
    board_i2c.c
    board_adc.c
    board_can.c
    board_timer.c
    board_dma.c
    board_rtc.c
    board_fwdgt.c
```

不推荐将大规模资源表长期放在 `main.c`。

复杂初始化流程可以放在 HAL / Board Port 的手写初始化函数中，不应塞进资源 config。

---

## 13. 非目标

TF SDK 当前明确不追求:

- 多平台完全统一的外设参数语义
- 通用 IRQ 抽象
- 通用时钟 / 复位资源框架
- 通用 DMA 绑定框架
- 通用完整初始化 config
- 重新实现完整厂商 HAL
- 将应用彻底与具体平台参数书写方式隔离

---

## 14. 开发与评审检查项

后续开发或评审时，至少检查以下问题。

### 14.1 结构检查

- 该模块是否仍符合 `resource config + register + handle + runtime op`
- 是否把完整初始化策略错误塞进了 config
- 是否把本应属于 HAL / Board Port 的内容错误上推到了 DRV
- 是否把本应属于平台层的 IRQ / DMA / EXTI 逻辑塞进了公共 config
- HAL 实现是否优先使用官方库 API，而非直接操作寄存器

### 14.2 config 检查

- `IO config` 是否只描述 GPIO 初始化资源
- 非 IO 外设 config 是否只保留 `base`、`io_id`、`channel id` 等资源描述
- `uart_configs[]` 是否只额外引入了 `baudrate` 这一类板级静态参数
- `dma_configs[]` 的地址字段是否只保留 `periph_addr`、`memory_addr`，且类型与官方库一致
- 是否错误使用了厂商初始化结构体类型
- 是否存在多余的 TF 抽象宏二次映射
- 是否把 `UART baudrate` 之外的初始化策略，如 sample time、prescaler、filter 细节等写进了 config

### 14.3 register 检查

- 是否校验 `id`
- 是否防止重复注册
- 是否同步注册必要 IO
- 是否通过 HAL / Board Port 同步初始化父模块内部子资源
- 是否只有在全部必要步骤成功后才标记 `registered`
- 是否把时钟 / 复位动作错误留在 DRV 中
- 是否把内部子资源错误暴露成公开注册步骤
