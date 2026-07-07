/**
 * @file    module_ws2812.c
 * @brief   三路 SPI 驱动 WS2812 灯带
 *
 * =====================================================================
 * 一、WS2812 时序要求（datasheet）
 * =====================================================================
 *
 *       T0H = 220ns ~  380ns    逻辑 0 高电平
 *       T0L = 580ns ~    1μs    逻辑 0 低电平
 *       T1H = 580ns ~    1μs    逻辑 1 高电平
 *       T1L = 580ns ~    1μs    逻辑 1 低电平
 *       RES   ≥ 280μs           帧复位低电平
 *
 *       数据格式：24bit GRB，高位先发，单线 NRZ，典型 800Kbps
 *       级联：内部整形转发，RESET 作为帧边界
 *
 *       当前 SPI 编码方案等效 WS2812 位速率为：
 *         6.75MHz / 8 = 843.75Kbps
 *       该速率不是严格 800Kbps，但 0/1 码的高低电平宽度均落在
 *       datasheet 窗口内。最终需用逻辑分析仪确认 DIN 波形。
 *
 * =====================================================================
 * 二、时钟配置
 * =====================================================================
 *
 *       SYSCLK = 108MHz（IRC8M/2 × PLL ×27）
 *       AHB    = 108MHz（/1）
 *       APB2   = 108MHz（/1）  →  SPI0
 *       APB1   =  54MHz（/2）  →  SPI1, SPI2
 *
 *       SPI   总线    预分频      SCK        字节时间
 *       ─────────────────────────────────────────────
 *       SPI0  APB2    /16     6.75MHz       1.185μs
 *       SPI1  APB1    /8      6.75MHz       1.185μs
 *       SPI2  APB1    /8      6.75MHz       1.185μs
 *
 * =====================================================================
 * 三、引脚分配（GD32F103 HD，全部默认无重映射）
 * =====================================================================
 *
 *       SPI0 ─ PA5(SCK), PA7(MOSI)           ← 原 IMU_INT2 / T2_CH1
 *       SPI1 ─ PB13(SCK), PB15(MOSI)
 *       SPI2 ─ PB3(SCK),  PB5(MOSI)
 *
 *       不需要配置 MISO / NSS / CS 引脚。
 *
 *       待确认：
 *         1. SPI2 使用 PB3 作为 SCK，PB3 复位后默认是 JTAG TDO。
 *            若使用 SPI2，用户须在调用 tf_drv_spi_register(E_DRV_SPI_2) 之前
 *            自行关闭 JTAG 并保留 SWD：
 *
 *              rcu_periph_clock_enable(RCU_AF);
 *              gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
 *
 *            或者在系统初始化早期统一配置。
 *            若只用 SPI0/SPI1 则无需处理此问题。
 *
 *         2. 当前只进行外设基础配置和理论分析，不在 main() 中接入
 *            WS2812 刷新流程；后续波形验证阶段再调用 ws2812_init_all()
 *            或 tf_drv_spi_register()。
 *
 *       对应 IO 枚举（tf_board_config.h）：
 *         E_IO_S0_SCK, E_IO_S0_MO
 *         E_IO_S1_SCK, E_IO_S1_MO
 *         E_IO_S2_SCK, E_IO_S2_MO
 *
 * =====================================================================
 * 四、SPI 初始化参数（tf_hal_spi.c）
 * =====================================================================
 *
 *       spi_parameter_struct spi_para;
 *       spi_struct_para_init(&spi_para);
 *
 *       spi_para.device_mode           = SPI_MASTER;
 *       spi_para.trans_mode            = SPI_TRANSMODE_FULLDUPLEX;
 *       spi_para.frame_size            = SPI_FRAMESIZE_8BIT;
 *       spi_para.nss                   = SPI_NSS_SOFT;
 *       spi_para.endian                = SPI_ENDIAN_MSB;
 *       spi_para.clock_polarity_phase  = SPI_CK_PL_HIGH_PH_2EDGE;
 *       // SPI0 → SPI_PSC_16,  SPI1/SPI2 → SPI_PSC_8
 *       spi_para.prescale              = spi_get_prescale(base);
 *
 *       spi_init(spi_periph, &spi_para);
 *       spi_enable(spi_periph);
 *
 *       注意：TF SDK 架构下 SPI 初始化由 HAL 层完成，
 *       Application 层只需调用 tf_drv_spi_register()。
 *
 *       待修改：
 *         1. tf_hal_spi_init() 当前缺少 SPI 外设时钟使能。
 *            对照 GD32 官方 SPI 例程，spi_init() 前应按 base 使能：
 *              SPI0 → rcu_periph_clock_enable(RCU_SPI0)
 *              SPI1 → rcu_periph_clock_enable(RCU_SPI1)
 *              SPI2 → rcu_periph_clock_enable(RCU_SPI2)
 *
 *         2. 当前使用 SPI_TRANSMODE_FULLDUPLEX，但 WS2812 只需要 MOSI。
 *            MISO 不接外部信号时，RX 数据无效；需要确认 TX 期间
 *            RXNE/RXORERR 不影响连续发送，或后续改为 transmit-only /
 *            bidirectional transmit 模式并清理接收状态。
 *
 * =====================================================================
 * 五、WS2812 编码（1 字节 = 1 逻辑位）
 * =====================================================================
 *
 *       在 6.75MHz SPI 下，每个 SPI 字节 = 1.185μs：
 *
 *         逻辑 0 = 0xC0（0b11000000）
 *           → 296ns HIGH + 888ns LOW
 *           T0H=296ns ✓  T0L=888ns ✓
 *
 *         逻辑 1 = 0xF0（0b11110000）
 *           → 592ns HIGH + 592ns LOW
 *           T1H=592ns ✓  T1L=592ns ✓
 *
 *         数据帧：24 字节 / LED，高位（G7）先发
 *
 *         编码函数：
 *           static void ws2812_encode(uint8_t *buf, uint32_t grb)
 *           {
 *               for (int bit = 23, out = 0; bit >= 0; bit--, out++)
 *                   buf[out] = (grb & ((uint32_t)1 << bit)) ? 0xF0 : 0xC0;
 *           }
 *
 *       待修改：
 *         当前文件下方的 ws2812_encode() 实现使用 buf[i] 写入，实际会
 *         让 frame[0] 对应 bit0，发送顺序变为低位先发；后续进入功能
 *         实现时必须按上面的 out 顺序修正。
 *
 *       RESET：连续发送 N 字节 0x00，MOSI 保持 LOW 实现复位。
 *       GD32 SPI 无间隙连续发 0x00，每字节 1.185μs：
 *         ceil(280μs / 1.185μs) = 237 字节 ≈ 281μs ≥ 280μs ✓
 *
 * =====================================================================
 * 六、使用流程（TF SDK 架构）
 * =====================================================================
 *
 *    1. 注册 SPI
 *         tf_drv_spi_register(E_DRV_SPI_0);   // 返回 handle
 *         tf_drv_spi_register(E_DRV_SPI_1);
 *         tf_drv_spi_register(E_DRV_SPI_2);
 *
 *    2. 编码颜色数据
 *         uint8_t frame[LED_COUNT * 24];
 *         for (each led) ws2812_encode(frame + led*24, color);
 *
 *    3. 发送（轮询）
 *         tf_drv_spi_tx(h_spi0, frame, LED_COUNT * 24);
 *         // MOSI 停在最后一位 LOW
 *
 *    4. RESET（≥280μs LOW）
 *         uint8_t reset = 0x00;
 *         for (int i = 0; i < 237; i++)
 *             tf_drv_spi_tx(h_spi, &reset, 1);
 *
 *    5. 循环刷新
 *         while (1) {
 *             update_colors();
 *             tf_drv_spi_tx(h_spi0, frame0, len);
 *             tf_drv_spi_tx(h_spi1, frame1, len);
 *             tf_drv_spi_tx(h_spi2, frame2, len);
 *             // RESET 在 ws2812_send_frame() 内自动追加
 *         }
 *
 *       提示：三路 SPI 可轮换发送，一路的 RESET 期间发另一路，
 *       减少空闲等待时间。
 *
 * =====================================================================
 * 七、帧率计算
 * =====================================================================
 *
 *       单路帧耗时 = RESET + N × 24 × 1.185μs
 *
 *       帧率 ≈ 1 / (280μs + N × 28.44μs)
 *
 *       N= 7  → 479μs  → 2.1KHz    N= 25 →  991μs → 1.0KHz
 *       N=14  → 678μs  → 1.5KHz    N= 50 → 1.70ms → 588Hz
 *       N=20  → 849μs  → 1.2KHz    N=100 → 3.12ms → 320Hz
 *
 * =====================================================================
 * 八、注意事项
 * =====================================================================
 *
 *    1. SPI 使用 CPOL=1, CPHA=1（Mode 3），SCK 空闲保持 HIGH。
 *       WS2812 不连接 SCK，实际使用 MOSI 上的 NRZ 波形。
 *       CPOL 只决定 SCK 空闲电平，不决定 MOSI 空闲电平。
 *       MOSI 不发数据时通常停在上次最后一位电平；0xC0/0xF0 的
 *       最后一位均为 LOW，因此一帧结束后 MOSI 理论上保持 LOW，
 *       可用于满足 RESET 低电平。
 *
 *    2. 三路 SPI 各自独立，SCK 和 MOSI 独立运行，
 *       互不影响，可同时或交替发送。
 *
 *    3. WS2812 级联：第一颗灯珠接收前 24bit 后锁定自身数据，
 *       后续 24bit 经内部整形转发至下一颗，直至 RESET 归零。
 *
 *    4. 当前不使用 DMA，发送期间 CPU 轮询等待。
 *       若灯珠数量大导致帧率受 CPU 阻塞影响，可后续加 DMA。
 *       当前 tf_board_dma.c 的 DMA 通道按 UART 配置，不能直接复用
 *       给 SPI WS2812；后续若启用 SPI DMA，需要单独规划 DMA0/DMA1
 *       通道，避免与 USART DMA 冲突。
 *
 *    5. MISO 引脚未配置，接收寄存器数据无效。
 *       当前发送函数尚未主动读取/丢弃 RX 数据，FULLDUPLEX 模式下
 *       需要关注接收溢出标志；后续应实测或调整 SPI 传输模式。
 *
 *    6. RESET 通过追加 237 字节 0x00 实现（≈281μs LOW），
 *       不依赖 SysTick / tf_delay_us 等时间基准。
 *       237 字节是 ceil(280μs / 1.185μs) 算出的安全值，
 *       如需加速可自行减少。
 *
 * =====================================================================
 * 九、本阶段需要记录的更改点（仅硬件基础配置和理论分析）
 * =====================================================================
 *
 *    A. 必改硬件配置：
 *       - 在 tf_hal_spi.c 中为 SPI0/SPI1/SPI2 增加 RCU_SPIx 时钟使能。
 *
 *    B. 需确认硬件/引脚：
 *       - SPI2 PB3 可能与 JTAG 冲突，使用 SPI2 前需关闭 JTAG 或
 *         改用 SPI0/SPI1。
 *       - 3.3V MCU MOSI 驱动 5V WS2812 时，VIH=0.7VDD 可能不满足；
 *         需要电平转换或确认实际灯珠输入阈值。
 *
 *    C. 需修正理论/代码一致性：
 *       - ws2812_encode() 当前写入顺序会导致低位先发，后续功能实现
 *         必须修正为 frame[0] 发送 bit23。
 *       - 当前等效速率为 843.75Kbps，虽然满足时序窗口，但需实测。
 *       - FULLDUPLEX + MISO 未接时需确认 RX 溢出不影响 TX，或改模式。
 *
 *    D. 暂不纳入本阶段：
 *       - 不修改 DMA 通道表。
 *       - 不接入 main() 刷新流程。
 *       - 不实现完整颜色缓存、动画、亮度、并发刷新逻辑。
 *
 * =====================================================================
 */



#include "module_ws2812.h"
#include "tf_board_spi.h"

#include <stdint.h>
/* =====================================================================
 * 用户配置
 * ===================================================================== */

/* 每路灯带的灯珠数量（按需修改） */
#define WS2812_LED_COUNT     7

/* 编码字节：逻辑 0 / 逻辑 1 */
#define WS2812_CODE_0        0xC0
#define WS2812_CODE_1        0xF0

/* RESET 所需 0x00 字节数（ceil(280μs / 1.185μs) ≥ 280μs） */
#define WS2812_RESET_BYTES   237

/* =====================================================================
 * WS2812 颜色编码（GRB，24bit，MSB first）
 * ===================================================================== */

static void ws2812_encode(uint8_t *buf, uint32_t grb_color)
{
    for (int bit = 23, out = 0; bit >= 0; bit--, out++) {
        buf[out] = (grb_color & ((uint32_t)1 << bit)) ? WS2812_CODE_1 : WS2812_CODE_0;
    }
}

/* =====================================================================
 * 初始化三路 SPI
 * ===================================================================== */

void ws2812_init_all(void)
{
    tf_drv_spi_register(E_DRV_SPI_0);
    tf_drv_spi_register(E_DRV_SPI_1);
    tf_drv_spi_register(E_DRV_SPI_2);
}

/* =====================================================================
 * 发送一帧（单路）
 * ===================================================================== */

void ws2812_send_frame(tf_drv_spi_handle_t h_spi,
                       const uint8_t *frame, uint32_t led_count)
{
    tf_drv_spi_tx(h_spi, frame, led_count * 24);

    /* RESET：追加 237 字节 0x00 → MOSI LOW ≈ 281μs */
    uint8_t reset = 0x00;
    for (uint32_t i = 0; i < WS2812_RESET_BYTES; i++) {
        tf_drv_spi_tx(h_spi, &reset, 1);
    }
}
