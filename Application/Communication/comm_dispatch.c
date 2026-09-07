#include "comm_message.h"
#include "FreeRTOS.h"
#include "uart_transport.h"
#include "comm_dispatch.h"
#include "modbus_crc.h"
#include "task.h"
#include <string.h>

#define MODBUS_FRAME_MIN_LEN     4U
#define MODBUS_ADDR_SINGLE_SLAVE 1U
#define MODBUS_ADDR_WIND         2U
#define MODBUS_ADDR_TC           5U

static bool message_readdress(DATA_MARK_E data_mark, uint8_t address,
                              const uint8_t *data, uint16_t data_len)
{
    my_message_node_t node;

    if ((data == NULL) || (data_len < MODBUS_FRAME_MIN_LEN) ||
        (data_len > MY_MESSAGE_DATA_SIZE) ||
        (modbus_rtu_crc16(data, data_len) != 0U)) {
        return true;
    }

    node.data = pvPortMalloc(data_len);
    if (node.data == NULL) {
        return false;
    }

    memcpy(node.data, data, data_len);
    node.data[0] = address;
    modbus_rtu_append_crc(node.data, (uint32_t)data_len - 2U);
    node.data_len = data_len;
    node.data_mark = data_mark;
    if (xQueueSend(main_queue, &node, 0U) != pdPASS) {
        vPortFree(node.data);
        return false;
    }
    return true;
}

static bool slave_message_issue(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len)
{
    uint8_t address;

    if ((data == NULL) || (data_len < MODBUS_FRAME_MIN_LEN) ||
        (data[0] != MODBUS_ADDR_SINGLE_SLAVE)) {
        return true;
    }

    switch (data_mark) {
    case DATA_FROM_WIND:
        address = MODBUS_ADDR_WIND;
        break;
    case UART3_RX_MARK:
        address = UART3_MASTER_ADDR;
        break;
    case DATA_FROM_TC:
        address = MODBUS_ADDR_TC;
        break;
    default:
        return true;
    }

    return message_readdress(DATA_TO_MAIN, address, data, data_len);
}

bool main_message_issue(uint8_t *data, uint16_t data_len)
{
    DATA_MARK_E data_mark;

    if ((data == NULL) || (data_len < MODBUS_FRAME_MIN_LEN)) {
        return true;
    }

    switch (data[0]) {
    case MODBUS_ADDR_WIND:
        data_mark = DATA_TO_WIND;
        break;
    case UART3_MASTER_ADDR:
        data_mark = UART3_TX_MARK;
        break;
    case MODBUS_ADDR_TC:
        data_mark = DATA_TO_TC;
        break;
    default:
        return true;
    }

    return message_readdress(data_mark, MODBUS_ADDR_SINGLE_SLAVE, data, data_len);
}

static bool message_send_by_mark(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len)
{
    board_usart_id_e usart_id;

    switch (data_mark) {
    case DATA_TO_MAIN:
        usart_id = BOARD_USART_MAIN;
        break;
    case DATA_TO_WIND:
        usart_id = BOARD_USART_WIND;
        break;
    case UART3_TX_MARK:
        usart_id = BOARD_USART_RAIN;
        break;
    case DATA_TO_TC:
        usart_id = BOARD_USART_TC;
        break;
    case DATA_TO_4G:
    default:
        return false;
    }

    return board_usart_send(usart_id, data, data_len);
}

bool message_send(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len)
{
    return message_send_by_mark(data_mark, data, data_len);
}

static bool main_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_MAIN, data, data_len);
}

static bool wind_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_WIND, data, data_len);
}

static bool uart3_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(UART3_TX_MARK, data, data_len);
}

static bool tc_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_TC, data, data_len);
}

static bool wind_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_WIND, data, data_len);
}

static bool uart3_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(UART3_RX_MARK, data, data_len);
}

static bool tc_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_TC, data, data_len);
}

typedef struct
{
    bool (*func)(uint8_t *, uint16_t);
} node_consume_func_t;

node_consume_func_t node_consume_func[DATA_MARK_CNT] =
{
    [DATA_TO_MAIN] = { main_message_send },
    [DATA_TO_WIND] = { wind_message_send },
    [UART3_TX_MARK] = { uart3_message_send },
    [DATA_TO_TC] = { tc_message_send },
    [DATA_FROM_MAIN] = { main_message_issue },
    [DATA_FROM_WIND] = { wind_message_issue },
    [UART3_RX_MARK] = { uart3_message_issue },
    [DATA_FROM_TC] = { tc_message_issue },
};

bool node_analyze(my_message_node_t node)
{
    if (node.data == NULL || node.data_len == 0U || node.data_len > MY_MESSAGE_DATA_SIZE) {
        return true;
    }
    if ((node.data_mark >= 0) && (node.data_mark < DATA_MARK_CNT) &&
        (node_consume_func[node.data_mark].func != NULL)) {
        return node_consume_func[node.data_mark].func(node.data, node.data_len);
    }
    return true;
}



void queue_pack_read(void *pvParameters)
{
    my_message_node_t node;
    (void)pvParameters;

    for (;;) {
        if ((xQueuePeek(main_queue, &node, portMAX_DELAY) == pdPASS) &&
            node_analyze(node)) {
            if (xQueueReceive(main_queue, &node, 0U) == pdPASS) {
                vPortFree(node.data);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
