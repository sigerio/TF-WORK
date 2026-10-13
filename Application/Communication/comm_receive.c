#include "comm_receive.h"
#include "comm_dispatch.h"
#include "modbus_crc.h"
#include "ringbuf.h"
#include "task.h"
#include <string.h>

#define RX_FIFO_SIZE 1024U
#define RX_FRAME_SLOTS 8U

typedef struct
{
    board_usart_id_e usart_id;
    DATA_MARK_E data_mark;
    ringbuf_t fifo;
    uint8_t storage[RX_FIFO_SIZE];
    uint8_t frame[MY_MESSAGE_DATA_SIZE];
    uint16_t lengths[RX_FRAME_SLOTS];
    uint16_t length;
    uint16_t last_byte_us;
    uint16_t gap_us;
    uint16_t byte_gap_us;
    uint16_t character_us;
    uint8_t head;
    uint8_t count;
    bool active;
    bool invalid;
    volatile uint32_t dropped;
} rx_channel_t;

static rx_channel_t channels[] = {
    {.usart_id = BOARD_USART_TC, .data_mark = DATA_FROM_TC},
    {.usart_id = BOARD_USART_WIND, .data_mark = DATA_FROM_WIND},
    {.usart_id = BOARD_USART_RAIN, .data_mark = DATA_FROM_RAIN},
    {.usart_id = BOARD_USART_WATER, .data_mark = DATA_FROM_WATER},
};
static volatile uint32_t invalid_frames;

static rx_channel_t *rx_channel_find(board_usart_id_e usart_id)
{
    for (uint32_t index = 0; index < sizeof(channels) / sizeof(channels[0]); index++)
    {
        if (channels[index].usart_id == usart_id)
        {
            return &channels[index];
        }
    }
    return NULL;
}

void rx_frame_init(void)
{
    for (uint32_t index = 0; index < sizeof(channels) / sizeof(channels[0]); index++)
    {
        rx_channel_t *channel = &channels[index];
        uint32_t baudrate = board_usart_configs[channel->usart_id].baudrate;
        ringbuf_init(&channel->fifo, channel->storage, sizeof(channel->storage));
        channel->character_us = (uint16_t)((10000000U + baudrate - 1U) / baudrate);
        channel->gap_us = board_rtu_gap_us(channel->usart_id);
        channel->byte_gap_us = (uint16_t)(channel->character_us +
                                          ((baudrate > 19200U) ? 750U : (15000000U + baudrate - 1U) / baudrate) +
                                          RX_TIMING_MARGIN_US);
    }
}

static void rx_frame_finish(rx_channel_t *channel)
{
    if (!channel->invalid && channel->length >= 4U &&
        channel->count < RX_FRAME_SLOTS &&
        ringbuf_free(&channel->fifo) >= channel->length)
    {
        ringbuf_write(&channel->fifo, channel->frame, channel->length);
        channel->lengths[(channel->head + channel->count) % RX_FRAME_SLOTS] = channel->length;
        channel->count++;
    }
    else
    {
        channel->dropped++;
    }
    channel->length = 0U;
    channel->active = false;
    channel->invalid = false;
}

void board_usart_rx_byte(board_usart_id_e usart_id, uint8_t byte)
{
    rx_channel_t *channel = rx_channel_find(usart_id);
    uint16_t now = board_comm_time_us();
    if (channel == NULL)
    {
        return;
    }
    if (channel->active)
    {
        uint16_t elapsed = (uint16_t)(now - channel->last_byte_us);
        if (elapsed >= channel->gap_us + channel->character_us)
        {
            rx_frame_finish(channel);
        }
        else if (elapsed > channel->byte_gap_us)
        {
            channel->invalid = true;
        }
    }
    channel->active = true;
    channel->last_byte_us = now;
    if (channel->length < sizeof(channel->frame))
    {
        channel->frame[channel->length++] = byte;
    }
    else
    {
        channel->invalid = true;
    }
}

void board_usart_rx_error(board_usart_id_e usart_id)
{
    rx_channel_t *channel = rx_channel_find(usart_id);
    if (channel != NULL)
    {
        channel->invalid = true;
        channel->active = true;
        channel->last_byte_us = board_comm_time_us();
    }
}

void board_usart_rx_tick(uint16_t now)
{
    for (uint32_t index = 0; index < sizeof(channels) / sizeof(channels[0]); index++)
    {
        rx_channel_t *channel = &channels[index];
        if (channel->active && (uint16_t)(now - channel->last_byte_us) >= channel->gap_us)
        {
            rx_frame_finish(channel);
        }
    }
}

static uint16_t rx_frame_read(rx_channel_t *channel, uint8_t *data)
{
    uint16_t length = 0U;
    taskENTER_CRITICAL();
    if (channel->count != 0U)
    {
        length = channel->lengths[channel->head];
        ringbuf_read_exact(&channel->fifo, data, length);
        channel->head = (uint8_t)((channel->head + 1U) % RX_FRAME_SLOTS);
        channel->count--;
    }
    taskEXIT_CRITICAL();
    return length;
}

static void rx_frame_submit(DATA_MARK_E data_mark, const uint8_t *data, uint16_t length)
{
    my_message_node_t node;
    bool submitted = false;
    /* Filter master-side addresses; downstream sensors reply at address 1. */
    if (length < 4U || length > MY_MESSAGE_DATA_SIZE ||
        (data_mark == DATA_FROM_MAIN &&
         !((data[0] >= 2U && data[0] <= 5U) || data[0] == 0xB4U)) ||
        modbus_rtu_crc16(data, length) != 0U)
    {
        invalid_frames++;
        return;
    }
    comm_log_frame(data_mark, data, length);
    do
    {
        node.data = pvPortMalloc(length);
        if (node.data == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    } while (node.data == NULL);
    memcpy(node.data, data, length);
    node.data_len = length;
    node.data_mark = data_mark;
    while (!submitted)
    {
        taskENTER_CRITICAL();
        if (uxQueueSpacesAvailable(main_queue) > 1U)
        {
            submitted = xQueueSend(main_queue, &node, 0U) == pdPASS;
        }
        taskEXIT_CRITICAL();
        if (!submitted)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void communication_rx_task(void *pvParameters)
{
    uint8_t data[MY_MESSAGE_DATA_SIZE];
    uint16_t length;
    (void)pvParameters;
    for (;;)
    {
        if (board_usart_main_dma_rx_fetch(data, &length))
        {
            rx_frame_submit(DATA_FROM_MAIN, data, length);
        }
        for (uint32_t index = 0; index < sizeof(channels) / sizeof(channels[0]); index++)
        {
            length = rx_frame_read(&channels[index], data);
            if (length != 0U)
            {
                rx_frame_submit(channels[index].data_mark, data, length);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
