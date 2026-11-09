#include "uart_transport.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_misc.h"
#include "gd32f10x_usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define BOARD_USART1_DMA_RX_BUFFER_SIZE UART_FRAME_MAX_SIZE
#define BOARD_USART1_DMA_TX_BUFFER_SIZE UART_FRAME_MAX_SIZE

static uint8_t usart1_dma_rx_buffer[BOARD_USART1_DMA_RX_BUFFER_SIZE];
/* ponytail: 主控保留一帧待取；主控改为流水发送时扩展为完整帧 FIFO。 */
static uint8_t usart1_dma_rx_pending_buffer[UART_FRAME_MAX_SIZE];
static volatile uint16_t usart1_dma_rx_pending_len;
static volatile uint8_t usart1_dma_rx_pending_ready;
static uint8_t usart1_dma_tx_buffer[BOARD_USART1_DMA_TX_BUFFER_SIZE];
static volatile uint8_t usart1_dma_tx_busy;
static uint16_t usart_tx_end_us[BOARD_USART_COUNT];
static bool usart_tx_wait_gap[BOARD_USART_COUNT];

static void board_usart_direction_set(const board_usart_config_t *config, FlagStatus level)
{
    if (config->direction_pin != BOARD_PIN_NONE) {
        gpio_bit_write(BOARD_PIN_PORT(config->direction_pin), BOARD_PIN_PIN(config->direction_pin), level);
        if (level == SET) {
            uint16_t started = board_comm_time_us();
            /* Allow the isolated DIR signal to settle before the first start bit. */
            while ((uint16_t)(board_comm_time_us() - started) < RS485_TX_SETUP_US) {
            }
        }
    }
}

static void board_usart_main_dma_tx_init(void)
{
    const board_usart_config_t *usart_config = &board_usart_configs[BOARD_USART_MAIN];
    const board_dma_ref_t tx_dma_ref = usart_config->tx_dma;
    const board_dma_config_t *dma_config = &board_dma_configs[tx_dma_ref.dma];
    const board_dma_channel_config_t *dma_channel_config = BOARD_DMA_CH(tx_dma_ref);
    dma_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(dma_config->clock);
    dma_deinit(dma_config->controller, dma_channel_config->channel);
    dma_struct_para_init(&dma_init_struct);
    dma_init_struct.direction = dma_channel_config->direction;
    dma_init_struct.memory_addr = (uint32_t)usart1_dma_tx_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = 0U;
    dma_init_struct.periph_addr = dma_channel_config->peripheral_address;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(dma_config->controller, dma_channel_config->channel, &dma_init_struct);
    usart1_dma_tx_busy = 0U;
}

void board_usart_main_dma_rx_init(void)
{
    const board_usart_config_t *usart_config = &board_usart_configs[BOARD_USART_MAIN];
    const board_dma_ref_t rx_dma_ref = usart_config->rx_dma;
    const board_dma_config_t *dma_config = &board_dma_configs[rx_dma_ref.dma];
    const board_dma_channel_config_t *dma_channel_config = BOARD_DMA_CH(rx_dma_ref);
    dma_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(dma_config->clock);
    dma_deinit(dma_config->controller, dma_channel_config->channel);
    dma_struct_para_init(&dma_init_struct);
    dma_init_struct.direction = dma_channel_config->direction;
    dma_init_struct.memory_addr = (uint32_t)usart1_dma_rx_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = BOARD_USART1_DMA_RX_BUFFER_SIZE;
    dma_init_struct.periph_addr = dma_channel_config->peripheral_address;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(dma_config->controller, dma_channel_config->channel, &dma_init_struct);
    dma_circulation_disable(dma_config->controller, dma_channel_config->channel);
    usart1_dma_rx_pending_len = 0U;
    usart1_dma_rx_pending_ready = 0U;
    dma_channel_enable(dma_config->controller, dma_channel_config->channel);
    usart_dma_receive_config(usart_config->peripheral, USART_RECEIVE_DMA_ENABLE);
    usart_interrupt_enable(usart_config->peripheral, USART_INT_IDLE);
    nvic_irq_enable(USART1_IRQn, 5U, 0U);
}

static bool board_usart_tx_ready(board_usart_id_e usart_id)
{
    if (usart_tx_wait_gap[usart_id]) {
        if ((uint16_t)(board_comm_time_us() - usart_tx_end_us[usart_id]) < board_rtu_gap_us(usart_id)) {
            return false;
        }
        usart_tx_wait_gap[usart_id] = false;
    }
    return true;
}

bool board_usart_main_dma_send(const uint8_t *data, uint16_t data_len)
{
    const board_usart_config_t *usart_config = &board_usart_configs[BOARD_USART_MAIN];
    const board_dma_ref_t tx_dma_ref = usart_config->tx_dma;
    const board_dma_config_t *dma_config = &board_dma_configs[tx_dma_ref.dma];
    const board_dma_channel_config_t *dma_channel_config = BOARD_DMA_CH(tx_dma_ref);

    board_usart_main_dma_tx_poll();
    if ((data == NULL) || (data_len == 0U) ||
        (data_len > BOARD_USART1_DMA_TX_BUFFER_SIZE) ||
        (usart1_dma_tx_busy != 0U) || !board_usart_tx_ready(BOARD_USART_MAIN)) {
        return false;
    }

    memcpy(usart1_dma_tx_buffer, data, data_len);
    usart_receive_config(USART1, USART_RECEIVE_DISABLE);
    board_usart_direction_set(usart_config, SET);
    dma_channel_disable(dma_config->controller, dma_channel_config->channel);
    dma_interrupt_flag_clear(dma_config->controller, dma_channel_config->channel, DMA_INT_FLAG_G);
    dma_memory_address_config(dma_config->controller, dma_channel_config->channel, (uint32_t)usart1_dma_tx_buffer);
    dma_transfer_number_config(dma_config->controller, dma_channel_config->channel, data_len);
    usart_interrupt_disable(usart_config->peripheral, USART_INT_TC);
    usart_flag_clear(usart_config->peripheral, USART_FLAG_TC);
    /* Publish busy only after DMA count and TC are ready for completion polling. */
    usart1_dma_tx_busy = 1U;
    usart_dma_transmit_config(usart_config->peripheral, USART_TRANSMIT_DMA_ENABLE);
    dma_channel_enable(dma_config->controller, dma_channel_config->channel);
    usart_interrupt_enable(USART1, USART_INT_TC);
    return true;
}

void board_usart_main_dma_rx_idle_irq_handler(void)
{
    const board_dma_ref_t ref = board_usart_configs[BOARD_USART_MAIN].rx_dma;
    const board_dma_config_t *config = &board_dma_configs[ref.dma];
    dma_channel_enum channel = BOARD_DMA_CH(ref)->channel;
    uint16_t remaining;
    uint16_t length;

    if (RESET == usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE)) {
        return;
    }

    usart_data_receive(USART1);
    remaining = (uint16_t)dma_transfer_number_get(config->controller, channel);
    length = BOARD_USART1_DMA_RX_BUFFER_SIZE - remaining;
    if (length != 0U && usart1_dma_rx_pending_ready == 0U) {
        memcpy(usart1_dma_rx_pending_buffer, usart1_dma_rx_buffer, length);
        usart1_dma_rx_pending_len = length;
        usart1_dma_rx_pending_ready = 1U;
    }
    dma_channel_disable(config->controller, channel);
    dma_memory_address_config(config->controller, channel, (uint32_t)usart1_dma_rx_buffer);
    dma_transfer_number_config(config->controller, channel, BOARD_USART1_DMA_RX_BUFFER_SIZE);
    dma_channel_enable(config->controller, channel);
}

bool board_usart_main_dma_rx_fetch(uint8_t *data, uint16_t *data_len)
{
    uint16_t pending_len;

    if ((data == NULL) || (data_len == NULL)) {
        return false;
    }

    taskENTER_CRITICAL();
    if ((usart1_dma_rx_pending_ready == 0U) || (usart1_dma_rx_pending_len == 0U)) {
        taskEXIT_CRITICAL();
        return false;
    }

    pending_len = usart1_dma_rx_pending_len;
    memcpy(data, usart1_dma_rx_pending_buffer, pending_len);
    usart1_dma_rx_pending_len = 0U;
    usart1_dma_rx_pending_ready = 0U;
    taskEXIT_CRITICAL();

    *data_len = pending_len;
    return true;
}

void board_usart_main_dma_tx_poll(void)
{
    const board_usart_config_t *usart_config = &board_usart_configs[BOARD_USART_MAIN];
    const board_dma_ref_t tx_dma_ref = usart_config->tx_dma;
    const board_dma_config_t *dma_config = &board_dma_configs[tx_dma_ref.dma];
    const board_dma_channel_config_t *dma_channel_config = BOARD_DMA_CH(tx_dma_ref);

    if (usart1_dma_tx_busy == 0U) {
        return;
    }

    if (dma_transfer_number_get(dma_config->controller, dma_channel_config->channel) != 0U ||
        RESET == usart_flag_get(usart_config->peripheral, USART_FLAG_TC)) {
        return;
    }

    usart_interrupt_disable(USART1, USART_INT_TC);
    dma_channel_disable(dma_config->controller, dma_channel_config->channel);
    dma_interrupt_flag_clear(dma_config->controller, dma_channel_config->channel, DMA_INT_FLAG_G);
    usart_dma_transmit_config(usart_config->peripheral, USART_TRANSMIT_DMA_DISABLE);
    usart_flag_clear(usart_config->peripheral, USART_FLAG_TC);
    board_usart_direction_set(usart_config, RESET);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_tx_end_us[BOARD_USART_MAIN] = board_comm_time_us();
    usart_tx_wait_gap[BOARD_USART_MAIN] = true;
    usart1_dma_tx_busy = 0U;
}

static void board_usart_init(board_usart_id_e usart_id, IRQn_Type irqn, uint8_t rx_irq_enable)
{
    const board_usart_config_t *config = &board_usart_configs[usart_id];

    board_gpio_init(config->tx_pin);
    board_gpio_init(config->rx_pin);
    rcu_periph_clock_enable(config->clock);
    usart_deinit(config->peripheral);
    usart_baudrate_set(config->peripheral, config->baudrate);
    usart_word_length_set(config->peripheral, USART_WL_8BIT);
    usart_stop_bit_set(config->peripheral, USART_STB_1BIT);
    usart_parity_config(config->peripheral, USART_PM_NONE);
    usart_transmit_config(config->peripheral, USART_TRANSMIT_ENABLE);
    usart_receive_config(config->peripheral, USART_RECEIVE_ENABLE);
    if (rx_irq_enable != 0U) {
        usart_interrupt_enable(config->peripheral, USART_INT_RBNE);
        usart_interrupt_enable(config->peripheral, USART_INT_ERR);
        nvic_irq_enable(irqn, 5U, 0U);
    }
    usart_enable(config->peripheral);
}

uint16_t board_comm_time_us(void)
{
    return (uint16_t)timer_counter_read(TIMER2);
}

uint16_t board_rtu_gap_us(board_usart_id_e usart_id)
{
    uint32_t baudrate = board_usart_configs[usart_id].baudrate;
    return (uint16_t)(((baudrate > 19200U) ? 1750U :
        (35000000U + baudrate - 1U) / baudrate) + RX_TIMING_MARGIN_US);
}

static void board_comm_timer_init(void)
{
    timer_parameter_struct config;
    uint32_t frequency = rcu_clock_freq_get(CK_APB1);
    if ((RCU_CFG0 & RCU_CFG0_APB1PSC) != RCU_APB1_CKAHB_DIV1) {
        frequency *= 2U;
    }
    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);
    timer_struct_para_init(&config);
    config.prescaler = (uint16_t)(frequency / 1000000U - 1U);
    config.period = 65535U;
    timer_init(TIMER2, &config);
    timer_channel_output_mode_config(TIMER2, TIMER_CH_0, TIMER_OC_MODE_TIMING);
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, 100U);
    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_CH0);
    timer_interrupt_enable(TIMER2, TIMER_INT_CH0);
    nvic_irq_enable(TIMER2_IRQn, 5U, 0U);
    timer_enable(TIMER2);
}

void board_comm_timer_irq_handler(void)
{
    if (timer_interrupt_flag_get(TIMER2, TIMER_INT_FLAG_CH0) != RESET) {
        uint16_t now = board_comm_time_us();
        timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_CH0);
        timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, (uint16_t)(now + 100U));
        board_usart_rx_tick(now);
    }
}

void uart_transport_init(void)
{
    board_usart_init(BOARD_USART_TC, USART0_IRQn, 1U);
    board_usart_init(BOARD_USART_MAIN, USART1_IRQn, 0U);
    board_usart_main_dma_tx_init();
    board_usart_main_dma_rx_init();
    board_usart_init(BOARD_USART_WIND, USART2_IRQn, 1U);
    board_usart_init(BOARD_USART_RAIN, UART4_IRQn, 1U);
    board_usart_init(BOARD_USART_WATER, UART3_IRQn, 1U);
    board_comm_timer_init();
}

#if defined(__CC_ARM)
__weak
#else
__attribute__((weak))
#endif
void board_usart_rx_byte(board_usart_id_e usart_id, uint8_t byte)
{
    /* ponytail: 默认钩子丢弃字节；接入协议模块时覆盖此函数。 */
    (void)usart_id;
    (void)byte;
}

void board_usart_rx_irq_handler(board_usart_id_e usart_id)
{
    const board_usart_config_t *config = &board_usart_configs[usart_id];

    uint32_t status = USART_STAT(config->peripheral);
    if ((status & (USART_STAT_ORERR | USART_STAT_NERR | USART_STAT_FERR | USART_STAT_PERR)) != 0U) {
        (void)USART_DATA(config->peripheral);
        board_usart_rx_error(usart_id);
    } else if ((status & USART_STAT_RBNE) != 0U) {
        board_usart_rx_byte(usart_id, (uint8_t)usart_data_receive(config->peripheral));
    }
}

bool board_usart_send(board_usart_id_e usart_id, const uint8_t *data, uint16_t data_len)
{
    const board_usart_config_t *config;
    uint16_t index;
    uint16_t started;
    bool completed = true;

    if (((unsigned)usart_id >= BOARD_USART_COUNT) || (data == NULL) ||
        (data_len == 0U) || (data_len > UART_FRAME_MAX_SIZE)) {
        return false;
    }

    if (usart_id == BOARD_USART_MAIN) {
        return board_usart_main_dma_send(data, data_len);
    }
    if (!board_usart_tx_ready(usart_id)) {
        return false;
    }

    config = &board_usart_configs[usart_id];
    usart_receive_config(config->peripheral, USART_RECEIVE_DISABLE);
    board_usart_direction_set(config, SET);
    usart_flag_clear(config->peripheral, USART_FLAG_TC);
    for (index = 0U; index < data_len; index++) {
        started = board_comm_time_us();
        while (RESET == usart_flag_get(config->peripheral, USART_FLAG_TBE)) {
            if ((uint16_t)(board_comm_time_us() - started) > board_rtu_gap_us(usart_id)) {
                completed = false;
                break;
            }
        }
        if (!completed) {
            break;
        }
        usart_data_transmit(config->peripheral, data[index]);
    }
    started = board_comm_time_us();
    while (RESET == usart_flag_get(config->peripheral, USART_FLAG_TC)) {
        if ((uint16_t)(board_comm_time_us() - started) > board_rtu_gap_us(usart_id)) {
            completed = false;
            break;
        }
    }
    board_usart_direction_set(config, RESET);
    usart_receive_config(config->peripheral, USART_RECEIVE_ENABLE);
    usart_tx_end_us[usart_id] = board_comm_time_us();
    usart_tx_wait_gap[usart_id] = true;
    return completed;
}

#if defined(__CC_ARM)
__weak
#else
__attribute__((weak))
#endif
void board_usart_rx_error(board_usart_id_e usart_id)
{
    (void)usart_id;
}

#if defined(__CC_ARM)
__weak
#else
__attribute__((weak))
#endif
void board_usart_rx_tick(uint16_t now)
{
    (void)now;
}
