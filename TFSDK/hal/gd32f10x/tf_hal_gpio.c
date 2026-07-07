/**
 * @file    tf_hal_gpio.c
 * @brief   GD32F10x GPIO + EXTI 硬件抽象实现
 *
 * 覆盖 tf_drv_io.h 中声明的 HAL 原语，直接使用 GD32F10x 标准固件库 API。
 * port 参数为 GPIO 基地址的 uint32_t 形式（如 GPIOA、GPIOB 等宏值）。
 *
 * ============ 模式参数约定 ============
 *
 * mode 参数直接使用 GD32F10x 固件库的合并模式常量（方向 + 上下拉编码合一）：
 *
 *   宏名                值     说明
 *   ──────────────────────────────────────────
 *   GPIO_MODE_AIN       0x00   模拟输入
 *   GPIO_MODE_IN_FLOATING 0x04 浮空输入
 *   GPIO_MODE_IPD       0x28   下拉输入
 *   GPIO_MODE_IPU       0x48   上拉输入
 *   GPIO_MODE_OUT_OD    0x14   开漏输出
 *   GPIO_MODE_OUT_PP    0x10   推挽输出
 *   GPIO_MODE_AF_OD     0x1C   复用开漏输出
 *   GPIO_MODE_AF_PP     0x18   复用推挽输出
 *
 * pull 参数在 GD32F10x 中已编码在 mode 内，本实现中忽略该字段。
 *
 * speed 参数使用 GD32F10x 速度常量：
 *   GPIO_OSPEED_10MHZ  (1)   输出最高速率 10MHz
 *   GPIO_OSPEED_2MHZ   (2)   输出最高速率 2MHz
 *   GPIO_OSPEED_50MHZ  (3)   输出最高速率 50MHz
 *
 * alternate 参数：
 *   GD32F10x 无 STM32 式逐引脚复用号（AF0~AF15），
 *   复用功能由 GPIO 模式（AF_PP / AF_OD）使能，引脚功能为硬件固定。
 *   如需引脚重映射，传入 AFIO_PCF0 重映射宏（如 GPIO_USART1_REMAP），
 *   本实现将自动调用 gpio_pin_remap_config() 完成配置。
 *
 * @note 本文件对应 GD32F10x 固件库 V2.7.0
 *
 * @see GD32F10x_Firmware_Library/Examples/GPIO/ 官方例程
 */

#include "../tf_hal_gpio.h"
#include "gd32f10x.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "../../framework/tf_debug.h"

/* ========== 内部辅助：GPIO 时钟使能 ========== */

/**
 * @brief 根据 GPIO 外设基地址使能对应时钟
 *
 * GD32F10x 的 GPIO 挂在 APB2 总线上，通过 RCU 控制时钟。
 * 支持 GPIOA ~ GPIOG（A~G 共 7 组）。
 *
 * @param gpio_periph  GPIO 端口基地址（如 GPIOA、GPIOB）
 */
static void gd32_gpio_clock_enable(uint32_t gpio_periph)
{
    if (gpio_periph == GPIOA) {
        rcu_periph_clock_enable(RCU_GPIOA);
        return;
    }
    if (gpio_periph == GPIOB) {
        rcu_periph_clock_enable(RCU_GPIOB);
        return;
    }
    if (gpio_periph == GPIOC) {
        rcu_periph_clock_enable(RCU_GPIOC);
        return;
    }
    if (gpio_periph == GPIOD) {
        rcu_periph_clock_enable(RCU_GPIOD);
        return;
    }
    if (gpio_periph == GPIOE) {
        rcu_periph_clock_enable(RCU_GPIOE);
        return;
    }
    if (gpio_periph == GPIOF) {
        rcu_periph_clock_enable(RCU_GPIOF);
        return;
    }
    if (gpio_periph == GPIOG) {
        rcu_periph_clock_enable(RCU_GPIOG);
        return;
    }
    /* 未知 GPIO 端口 —— 不使能时钟，保留 debug 追踪 */
    TF_DEBUG("HAL", "unknown GPIO port: 0x%08lX\n", (unsigned long)gpio_periph);
}

/* ========== GPIO 初始化 ========== */

void tf_hal_gpio_init(uintptr_t port, uint16_t pin, uint8_t mode, uint8_t pull, uint8_t speed, uint32_t alternate, uint8_t init_state)
{
    uint32_t gpio_periph = (uint32_t)port;
    uint32_t gpio_pin    = (uint32_t)pin;

    TF_DEBUG("HAL", "port=0x%08lX pin=0x%04X mode=0x%02X pull=%u speed=%u alt=0x%08lX init=%u\n",
             (unsigned long)gpio_periph, (unsigned)pin, (unsigned)mode,
             (unsigned)pull, (unsigned)speed, (unsigned long)alternate,
             (unsigned)init_state);

    /* ── 1. 使能 GPIO 时钟 ── */
    gd32_gpio_clock_enable(gpio_periph);

    /* ── 2. 构造 GD32 合并模式值 ──
     *
     * 对于 GD32F10x，mode 参数已经包含了方向 + 上下拉信息
     * （如 GPIO_MODE_IPU、GPIO_MODE_OUT_PP 等），
     * 因此直接透传即可。pull 参数内含于 mode，不做额外处理。
     */
    uint32_t gd_mode = (uint32_t)mode;

    /* ── 3. 构造 GD32 速率值 ──
     *
     * speed 参数直接使用 GD32F10x 速率常量，透传至固件库。
     */
    uint32_t gd_speed = (uint32_t)speed;

    /* ── 4. 调用固件库 GPIO 初始化 ──
     *
     * gpio_init() 会一次性完成：
     *   - GPIO_CTL0/CTL1 模式寄存器的写入
     *   - IPD/IPU 模式下 OCTL 的初始上下拉配置
     * 参考 GD32 官方 Running_led / Keyboard_polling_mode 例程。
     */
    gpio_init(gpio_periph, gd_mode, gd_speed, gpio_pin);

    /* ── 5. 输出模式：设置初始电平 ──
     *
     * GD32 gpio_init() 只配置模式和速率，不设置输出电平。
     * 需根据 init_state 显式写入 BOP/BC 寄存器。
     */
    if ((gd_mode == GPIO_MODE_OUT_PP) || (gd_mode == GPIO_MODE_OUT_OD) ||
        (gd_mode == GPIO_MODE_AF_PP) || (gd_mode == GPIO_MODE_AF_OD)) {
        if (init_state) {
            gpio_bit_set(gpio_periph, gpio_pin);
        } else {
            gpio_bit_reset(gpio_periph, gpio_pin);
        }
    }

    /* ── 6. 复用模式：执行引脚重映射（如需） ──
     *
     * 当 mode 为 AF_PP/AF_OD 且 alternate 指定了重映射宏时，
     * 调用 gpio_pin_remap_config() 将外设功能映射到目标引脚。
     * alternate = 0 表示使用默认引脚映射，不做 remap。
     */
    if ((alternate != 0U) &&
        ((gd_mode == GPIO_MODE_AF_PP) || (gd_mode == GPIO_MODE_AF_OD))) {
        gpio_pin_remap_config(alternate, ENABLE);
        TF_DEBUG("HAL", "remap configured: 0x%08lX\n", (unsigned long)alternate);
    }
}

/* ========== GPIO 输出 ========== */

void tf_hal_gpio_write(uintptr_t port, uint16_t pin, uint8_t level)
{
    uint32_t gpio_periph = (uint32_t)port;
    uint32_t gpio_pin    = (uint32_t)pin;

    /* gpio_bit_write() 内部根据 SET/RESET 选择 BOP/BC 寄存器，原子操作 */
    gpio_bit_write(gpio_periph, gpio_pin, level ? SET : RESET);
}

/* ========== GPIO 输入读取 ========== */

uint8_t tf_hal_gpio_read(uintptr_t port, uint16_t pin)
{
    uint32_t gpio_periph = (uint32_t)port;
    uint32_t gpio_pin    = (uint32_t)pin;

    /* gpio_input_bit_get() 返回 SET(1) 或 RESET(0) */
    return (gpio_input_bit_get(gpio_periph, gpio_pin) == SET) ? 1U : 0U;
}
