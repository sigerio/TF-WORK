#include "comm_message.h"
#include "FreeRTOS.h"
#include "uart_transport.h"
#include "comm_dispatch.h"
#include "modbus_crc.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include <string.h>

#define MODBUS_FRAME_MIN_LEN     4U
#define MODBUS_ADDR_SINGLE_SLAVE 1U
#define MODBUS_ADDR_WIND         2U
#define MODBUS_ADDR_RAIN         3U
#define MODBUS_ADDR_WATER        4U
#define MODBUS_ADDR_TC           5U
#define MODBUS_ADDR_WEATHER      0xB4U
#define WEATHER_FAULT_FUNCTION   0x41U
#define WEATHER_FAULT_FRAME_LEN  9U

static weather_fault_u weather_fault;
static bool weather_fault_received;

bool weather_fault_get(weather_fault_u *fault)
{
    bool received;

    if (fault == NULL) {
        return false;
    }
    taskENTER_CRITICAL();
    received = weather_fault_received;
    if (received) {
        *fault = weather_fault;
    }
    taskEXIT_CRITICAL();
    return received;
}

static void weather_fault_receive(const uint8_t *data, uint16_t data_len)
{
    uint32_t raw;

    if (data_len != WEATHER_FAULT_FRAME_LEN || data[1] != WEATHER_FAULT_FUNCTION ||
        data[2] != sizeof(raw) || modbus_rtu_crc16(data, data_len) != 0U) {
        return;
    }
    /* Both GD32 endpoints use little endian; payload starts at an unaligned address. */
    memcpy(&raw, &data[3], sizeof(raw));
    taskENTER_CRITICAL();
    weather_fault.raw = raw;
    weather_fault_received = true;
    taskEXIT_CRITICAL();
}

void comm_log_frame(DATA_MARK_E data_mark, const uint8_t *data, uint16_t data_len)
{
    static const char hex[] = "0123456789ABCDEF";
    static char payload[3U * MY_MESSAGE_DATA_SIZE];
    const char *label;
    unsigned read_offset;
    unsigned write_offset;
    unsigned available;

    if (data == NULL || data_len < MODBUS_FRAME_MIN_LEN ||
        data_len > MY_MESSAGE_DATA_SIZE || modbus_rtu_crc16(data, data_len) != 0U) {
        return;
    }
    switch (data_mark) {
    case DATA_FROM_MAIN: label = "UART1 RX"; break;
    case DATA_FROM_WIND: label = "UART2 RX"; break;
    case DATA_FROM_RAIN: label = "UART4 RX"; break;
    case DATA_FROM_WATER: label = "UART3 RX"; break;
    case DATA_FROM_TC: label = "UART0 RX"; break;
    case DATA_TO_MAIN: label = "UART1 TX"; break;
    case DATA_TO_WIND: label = "UART2 TX"; break;
    case DATA_TO_RAIN: label = "UART4 TX"; break;
    case DATA_TO_WATER: label = "UART3 TX"; break;
    case DATA_TO_TC: label = "UART0 TX"; break;
    default: return;
    }

    vTaskSuspendAll();
    read_offset = _SEGGER_RTT.aUp[0].RdOff;
    write_offset = _SEGGER_RTT.aUp[0].WrOff;
    available = (read_offset > write_offset) ? read_offset - write_offset - 1U :
        _SEGGER_RTT.aUp[0].SizeOfBuffer - write_offset + read_offset - 1U;
    if (available >= 3U * data_len + 48U) {
        for (uint16_t index = 0U; index < data_len; index++) {
            payload[3U * index] = hex[data[index] >> 4];
            payload[3U * index + 1U] = hex[data[index] & 0x0FU];
            payload[3U * index + 2U] = ' ';
        }
        payload[3U * data_len - 1U] = '\0';
        unsigned uptime_ms = (unsigned)((uint64_t)xTaskGetTickCount() * 1000U / configTICK_RATE_HZ);
        SEGGER_RTT_printf(0, "[%u ms] [%s len=%u] %s\r\n",
                          uptime_ms, label, (unsigned)data_len, payload);
    }
    (void)xTaskResumeAll();
}

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
    case DATA_FROM_RAIN:
        address = MODBUS_ADDR_RAIN;
        break;
    case DATA_FROM_WATER:
        address = MODBUS_ADDR_WATER;
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
    case MODBUS_ADDR_WEATHER:
        weather_fault_receive(data, data_len);
        return true; /* One-way local notification: no reply or forwarding. */
    case MODBUS_ADDR_WIND:
        data_mark = DATA_TO_WIND;
        break;
    case MODBUS_ADDR_RAIN:
        data_mark = DATA_TO_RAIN;
        break;
    case MODBUS_ADDR_WATER:
        data_mark = DATA_TO_WATER;
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
    case DATA_TO_RAIN:
        usart_id = BOARD_USART_RAIN;
        break;
    case DATA_TO_WATER:
        usart_id = BOARD_USART_WATER;
        break;
    case DATA_TO_TC:
        usart_id = BOARD_USART_TC;
        break;
    case DATA_TO_4G:
    default:
        return false;
    }

    if (!board_usart_send(usart_id, data, data_len)) {
        return false;
    }
    comm_log_frame(data_mark, data, data_len);
    return true;
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

static bool rain_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_RAIN, data, data_len);
}

static bool water_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_WATER, data, data_len);
}

static bool tc_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_TC, data, data_len);
}

static bool wind_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_WIND, data, data_len);
}

static bool rain_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_RAIN, data, data_len);
}

static bool water_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_WATER, data, data_len);
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
    [DATA_TO_RAIN] = { rain_message_send },
    [DATA_TO_WATER] = { water_message_send },
    [DATA_TO_TC] = { tc_message_send },
    [DATA_FROM_MAIN] = { main_message_issue },
    [DATA_FROM_WIND] = { wind_message_issue },
    [DATA_FROM_RAIN] = { rain_message_issue },
    [DATA_FROM_WATER] = { water_message_issue },
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
