#include "DEBUG/debug_console.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DEBUG/debug_log.h"
#include "BOARD/board.h"
#include "COMM/link/com_link.h"
#include "gd32f10x_dma.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_rcu.h"
#include "gd32f10x_usart.h"
#include "COMMON/ringbuf.h"
#include "CONTROL/ws2812/ws2812_status.h"

/*
 * 调试打印口 — 与指定通信口复用，由宏选择。
 *
 * 由 DEBUG_ENABLE 控制总开关：定义时输出经 USART2 TX DMA 发出；
 * 未定义时所有函数为空操作，不占硬件。
 *
 * 固定使用 USART2（充电板通信口）。IEC104 开发端口启用时，该口由 IEC104
 * 独占；默认配置自动关闭打印，显式同时启用两者会触发编译错误。
 */

#if defined(DEBUG_ENABLE)

debug_ctrl_u debug_ctrl = {0};
/* 上位机命令接收 FIFO（RXNE 中断推入，供调试命令解析读取） */
#define DEBUG_CONSOLE_RX_FIFO_SIZE (256U)
static uint8_t s_rx_fifo_buf[DEBUG_CONSOLE_RX_FIFO_SIZE];
static ringbuf_t s_rx_fifo;

#define DEBUG_CONSOLE_COMMAND_SIZE  (16U)
static char s_command[DEBUG_CONSOLE_COMMAND_SIZE];
static uint8_t s_command_length;

/* TX 双缓冲（ping-pong）：一块在 DMA 发送，另一块继续写入，提升吞吐 */
#define DEBUG_CONSOLE_BUFFER_COUNT 2U
#define DEBUG_CONSOLE_BUFFER_SIZE 2048U
#define DEBUG_CONSOLE_IDLE_BUFFER 0xFFU
static volatile uint8_t active_buffer = DEBUG_CONSOLE_IDLE_BUFFER;
static uint8_t buffers[DEBUG_CONSOLE_BUFFER_COUNT][DEBUG_CONSOLE_BUFFER_SIZE];
static volatile uint16_t buffer_lengths[DEBUG_CONSOLE_BUFFER_COUNT];

static void debug_console_tx_start(const board_usart_config_t *usart, uint8_t buffer_index)
{
    const board_dma_config_t *dma_ctl = &board_dma_configs[usart->tx_dma.dma];
    const board_dma_channel_config_t *dma = &dma_ctl->ch[usart->tx_dma.ch];

    dma_channel_disable(dma_ctl->controller, dma->channel);
    dma_periph_address_config(dma_ctl->controller, dma->channel, dma->peripheral_address);
    dma_memory_address_config(dma_ctl->controller, dma->channel, (uint32_t)buffers[buffer_index]);
    dma_transfer_number_config(dma_ctl->controller, dma->channel, buffer_lengths[buffer_index]);
    dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    active_buffer = buffer_index;
    dma_channel_enable(dma_ctl->controller, dma->channel);
}

static void debug_console_tx_kick(const board_usart_config_t *usart)
{
    uint8_t pending_buffer;

    if (active_buffer == DEBUG_CONSOLE_IDLE_BUFFER) {
        pending_buffer = (buffer_lengths[0] != 0U) ? 0U : 1U;
        if (buffer_lengths[pending_buffer] != 0U) {
            debug_console_tx_start(usart, pending_buffer);
        }
    }
}

static void debug_console_tx_complete(const board_usart_config_t *usart)
{
    const board_dma_config_t *dma_ctl = &board_dma_configs[usart->tx_dma.dma];
    const board_dma_channel_config_t *dma = &dma_ctl->ch[usart->tx_dma.ch];

    if (active_buffer == DEBUG_CONSOLE_IDLE_BUFFER) {
        return;
    }

    dma_channel_disable(dma_ctl->controller, dma->channel);
    dma_flag_clear(dma_ctl->controller, dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    buffer_lengths[active_buffer] = 0U;
    active_buffer = DEBUG_CONSOLE_IDLE_BUFFER;
    debug_console_tx_kick(usart);
}

void debug_console_tx_dma_irq(void)
{
    const board_usart_config_t *usart = &board_usart_configs[BOARD_USART_CHARGER];
    const board_dma_config_t *dma_ctl = &board_dma_configs[usart->tx_dma.dma];
    const board_dma_channel_config_t *dma = &dma_ctl->ch[usart->tx_dma.ch];

    if (dma_flag_get(dma_ctl->controller, dma->channel, DMA_FLAG_FTF) == RESET) {
        return;
    }

    debug_console_tx_complete(usart);
}

static void debug_console_tx_dma_irq_enable(const board_usart_config_t *usart)
{
    const board_dma_config_t *dma_ctl = &board_dma_configs[usart->tx_dma.dma];
    const board_dma_channel_config_t *dma = &dma_ctl->ch[usart->tx_dma.ch];

    dma_interrupt_enable(dma_ctl->controller, dma->channel, DMA_INT_FTF);
    nvic_irq_enable(DMA0_Channel1_IRQn, 1U, 3U);
}

/* 谁用谁初始化：调试口占用该串口，由本模块用官方库函数完成其硬件初始化。
 * 当前充电板协议模块未实现，USART2 暂无其他 owner；
 * 若后续该口由专门协议模块接管，请移除本函数的硬件初始化以避免重复配置。
 * TX 走 DMA0_CH1（USART2_TX），RX 走 RXNE 中断。 */
static void debug_console_hardware_init(const board_usart_config_t *usart)
{
    const board_gpio_config_t *tx_pin = &board_gpio_configs[usart->tx_pin];
    const board_gpio_config_t *rx_pin = &board_gpio_configs[usart->rx_pin];
    const board_dma_config_t *tx_dma_ctl = &board_dma_configs[usart->tx_dma.dma];
    const board_dma_channel_config_t *tx_dma = &tx_dma_ctl->ch[usart->tx_dma.ch];
    dma_parameter_struct dma_parameters;

    /* GPIO */
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(tx_pin->port, tx_pin->mode, tx_pin->speed, tx_pin->pin);
    gpio_bit_write(tx_pin->port, tx_pin->pin, tx_pin->initial_level ? SET : RESET);
    gpio_init(rx_pin->port, rx_pin->mode, rx_pin->speed, rx_pin->pin);

    /* USART */
    rcu_periph_clock_enable(usart->clock);
    usart_deinit(usart->peripheral);
    usart_baudrate_set(usart->peripheral, usart->baudrate);
    usart_word_length_set(usart->peripheral, USART_WL_8BIT);
    usart_stop_bit_set(usart->peripheral, USART_STB_1BIT);
    usart_parity_config(usart->peripheral, USART_PM_NONE);
    usart_hardware_flow_rts_config(usart->peripheral, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(usart->peripheral, USART_CTS_DISABLE);
    usart_receive_config(usart->peripheral, USART_RECEIVE_ENABLE);
    usart_transmit_config(usart->peripheral, USART_TRANSMIT_ENABLE);
    usart_enable(usart->peripheral);

    /* TX DMA（USART2_TX -> DMA0_CH1） */
    rcu_periph_clock_enable(tx_dma_ctl->clock);
    dma_struct_para_init(&dma_parameters);
    dma_parameters.periph_addr = tx_dma->peripheral_address;
    dma_parameters.memory_addr = 0U;
    dma_parameters.number = 0U;
    dma_parameters.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_parameters.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_parameters.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_parameters.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_parameters.direction = tx_dma->direction;
    dma_parameters.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_deinit(tx_dma_ctl->controller, tx_dma->channel);
    dma_init(tx_dma_ctl->controller, tx_dma->channel, &dma_parameters);
    dma_circulation_disable(tx_dma_ctl->controller, tx_dma->channel);
    dma_memory_to_memory_disable(tx_dma_ctl->controller, tx_dma->channel);
    dma_flag_clear(tx_dma_ctl->controller, tx_dma->channel, DMA_FLAG_G | DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_ERR);
    usart_dma_transmit_config(usart->peripheral, USART_TRANSMIT_DMA_ENABLE);
    debug_console_tx_dma_irq_enable(usart);
}

void debug_console_init(void)
{
    const board_usart_config_t *usart = &board_usart_configs[BOARD_USART_CHARGER];

    debug_console_hardware_init(usart);

    active_buffer = DEBUG_CONSOLE_IDLE_BUFFER;
    buffer_lengths[0] = 0U;
    buffer_lengths[1] = 0U;

    /* 使能 RXNE 中断：上位机下发命令时由 debug_console_irq() 收进 FIFO */
    ringbuf_init(&s_rx_fifo, s_rx_fifo_buf, sizeof(s_rx_fifo_buf));
    usart_interrupt_enable(usart->peripheral, USART_INT_RBNE);
    nvic_irq_enable(com_link_usart_irqn(usart->peripheral), 1U, 2U);
}

void debug_console_irq(void)
{
    const board_usart_config_t *usart = &board_usart_configs[BOARD_USART_CHARGER];

    if (usart_interrupt_flag_get(usart->peripheral, USART_INT_FLAG_RBNE) != RESET) {
        uint8_t byte = (uint8_t)usart_data_receive(usart->peripheral);

        (void)ringbuf_write(&s_rx_fifo, &byte, 1U);
    }
}


static void debug_console_write(const char *text, uint32_t length)
{
    const board_usart_config_t *usart = &board_usart_configs[BOARD_USART_CHARGER];
    uint8_t buffer_index;
    uint16_t offset;

    if ((text == NULL) || (length == 0U)) {
        return;
    }

    buffer_index = (active_buffer == DEBUG_CONSOLE_IDLE_BUFFER) ? 0U :
                   (uint8_t)(1U - active_buffer);
    offset = buffer_lengths[buffer_index];

    for (uint32_t index = 0U; index < length; index++) {
        if ((text[index] == '\n') && ((index == 0U) || (text[index - 1U] != '\r'))) {
            if ((uint32_t)offset + 2U > sizeof(buffers[buffer_index])) {
                break;
            }
            buffers[buffer_index][offset++] = (uint8_t)'\r';
        }
        if ((uint32_t)offset + 1U > sizeof(buffers[buffer_index])) {
            break;
        }
        buffers[buffer_index][offset++] = (uint8_t)text[index];
    }

    if (offset != buffer_lengths[buffer_index]) {
        buffer_lengths[buffer_index] = offset;
        debug_console_tx_kick(usart);
    }
}

int debug_printf(const char *format, ...)
{
    va_list arguments;
    char buffer[512];
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length > 0) {
        debug_console_write(buffer, (length < (int)sizeof(buffer)) ? (uint32_t)length : (uint32_t)(sizeof(buffer) - 1U));
    }

    return length;
}

static bool debug_console_parse_value(const char *text, uint8_t max_value, uint8_t *value)
{
    uint16_t parsed = 0U;
    bool has_digit = false;

    if ((text == NULL) || (value == NULL)) {
        return false;
    }

    while ((*text >= '0') && (*text <= '9')) {
        has_digit = true;
        parsed = (uint16_t)(parsed * 10U + (uint16_t)(*text - '0'));
        if (parsed > max_value) {
            return false;
        }
        text++;
    }

    if (!has_digit || (*text != '\0')) {
        return false;
    }

    *value = (uint8_t)parsed;
    return true;
}

static bool debug_console_apply_command(const char *command)
{
    const char *value_text;
    uint8_t value;

    if (command == NULL) {
        return false;
    }

    if (strncmp(command, "AC:", 3U) == 0)
    {
        value_text = &command[3];
        if (!debug_console_parse_value(value_text, 1U, &value))
        {
            return false;
        }
        debug_ctrl.bits.ac = value;
    }
    else if (strncmp(command, "IEC:", 4U) == 0)
    {
        value_text = &command[4];
        if (!debug_console_parse_value(value_text, 1U, &value))
        {
            return false;
        }
        debug_ctrl.bits.iec = value;
    }
    else if (strncmp(command, "WETH:", 5U) == 0)
    {
        value_text = &command[5];
        if (!debug_console_parse_value(value_text, 1U, &value))
        {
            return false;
        }
        debug_ctrl.bits.weth = value;
    }
    else if (strncmp(command, "WSE:", 4U) == 0)
    {
        value_text = &command[4];
        if (!debug_console_parse_value(value_text, WS2812_DEBUG_WARP_CHARGE, &value))
        {
            return false;
        }
        return module_ws2812_debug_select((ws2812_debug_effect_e)value);
    }
    else if (strncmp(command, "DOR:", 4U) == 0)
    {
        value_text = &command[4];
        if (!debug_console_parse_value(value_text, 1U, &value))
        {
            return false;
        }
        debug_ctrl.bits.dor = value;
    }
    else if (strncmp(command, "HEI:", 4U) == 0)
    {
        value_text = &command[4];
        if (!debug_console_parse_value(value_text, 1U, &value))
        {
            return false;
        }
        debug_ctrl.bits.hei = value;
    }
    else
    {
        return false;
    }

    return true;
}

void debug_console_task(void)
{
    uint8_t byte;

    while (ringbuf_available(&s_rx_fifo) > 0U) {
        __disable_irq();
        if (ringbuf_read_exact(&s_rx_fifo, &byte, 1U) != 1U) {
            __enable_irq();
            return;
        }
        __enable_irq();

        if ((byte == '\r') || (byte == '\n')) {
            if (s_command_length > 0U) {
                s_command[s_command_length] = '\0';
                (void)debug_console_apply_command(s_command);
                s_command_length = 0U;
            }
            continue;
        }

        if (s_command_length < (DEBUG_CONSOLE_COMMAND_SIZE - 1U)) {
            s_command[s_command_length++] = (char)byte;
        } else {
            s_command_length = 0U;
        }
    }
}

#else /* DEBUG_ENABLE 未定义 */

void debug_console_task(void)
{
}

void debug_console_init(void)
{
}

void debug_console_irq(void)
{
}

void debug_console_tx_dma_irq(void)
{
}

int debug_printf(const char *format, ...)
{
    (void)format;
    return 0;
}

#endif /* DEBUG_ENABLE */
