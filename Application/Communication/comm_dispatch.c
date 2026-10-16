#include "comm_message.h"
#include "FreeRTOS.h"
#include "uart_transport.h"
#include "comm_dispatch.h"
#include "modbus_crc.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include <string.h>

#define MODBUS_FRAME_MIN_LEN     4U
bool comm_address_allowed(DATA_MARK_E source, uint8_t address)
{
    switch (source) {
    case DATA_FROM_MAIN:
        return address == MODBUS_ADDR_MACHINE || address == MODBUS_ADDR_POLE ||
               address == MODBUS_ADDR_SOLAR || address == MODBUS_ADDR_MT80;
    case DATA_FROM_MACHINE:
    case DATA_FROM_POLE:
    case DATA_FROM_SOLAR:
        return address == MODBUS_ADDR_SINGLE_SLAVE;
    case DATA_FROM_MT80:
        return address == MODBUS_ADDR_MT80_SLAVE;
    default:
        return false;
    }
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
    case DATA_FROM_MACHINE: label = "UART2 RX"; break;
    case DATA_FROM_POLE: label = "UART3 RX"; break;
    case DATA_FROM_SOLAR: label = "UART4 RX"; break;
    case DATA_FROM_MT80: label = "UART0 RX"; break;
    case DATA_TO_MAIN: label = "UART1 TX"; break;
    case DATA_TO_MACHINE: label = "UART2 TX"; break;
    case DATA_TO_POLE: label = "UART3 TX"; break;
    case DATA_TO_SOLAR: label = "UART4 TX"; break;
    case DATA_TO_MT80: label = "UART0 TX"; break;
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
        !comm_address_allowed(data_mark, data[0])) {
        return true;
    }

    switch (data_mark) {
    case DATA_FROM_MACHINE:
        address = MODBUS_ADDR_MACHINE;
        break;
    case DATA_FROM_POLE:
        address = MODBUS_ADDR_POLE;
        break;
    case DATA_FROM_SOLAR:
        address = MODBUS_ADDR_SOLAR;
        break;
    case DATA_FROM_MT80:
        address = MODBUS_ADDR_MT80;
        break;
    default:
        return true;
    }

    return message_readdress(DATA_TO_MAIN, address, data, data_len);
}

bool main_message_issue(uint8_t *data, uint16_t data_len)
{
    DATA_MARK_E data_mark;
    uint8_t slave_address = MODBUS_ADDR_SINGLE_SLAVE;

    if ((data == NULL) || (data_len < MODBUS_FRAME_MIN_LEN)) {
        return true;
    }

    switch (data[0]) {
    case MODBUS_ADDR_MACHINE:
        data_mark = DATA_TO_MACHINE;
        break;
    case MODBUS_ADDR_POLE:
        data_mark = DATA_TO_POLE;
        break;
    case MODBUS_ADDR_SOLAR:
        data_mark = DATA_TO_SOLAR;
        break;
    case MODBUS_ADDR_MT80:
        data_mark = DATA_TO_MT80;
        slave_address = MODBUS_ADDR_MT80_SLAVE;
        break;
    default:
        return true;
    }

    return message_readdress(data_mark, slave_address, data, data_len);
}

static bool message_send_by_mark(DATA_MARK_E data_mark, uint8_t *data, uint16_t data_len)
{
    board_usart_id_e usart_id;

    switch (data_mark) {
    case DATA_TO_MAIN:
        usart_id = BOARD_USART_MAIN;
        break;
    case DATA_TO_MACHINE:
        usart_id = BOARD_USART_MACHINE;
        break;
    case DATA_TO_POLE:
        usart_id = BOARD_USART_POLE;
        break;
    case DATA_TO_SOLAR:
        usart_id = BOARD_USART_SOLAR;
        break;
    case DATA_TO_MT80:
        usart_id = BOARD_USART_MT80;
        break;
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

static bool machine_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_MACHINE, data, data_len);
}

static bool pole_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_POLE, data, data_len);
}

static bool mt80_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_MT80, data, data_len);
}

static bool machine_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_MACHINE, data, data_len);
}

static bool pole_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_POLE, data, data_len);
}

static bool mt80_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_MT80, data, data_len);
}

static bool solar_message_send(uint8_t *data, uint16_t data_len)
{
    return message_send(DATA_TO_SOLAR, data, data_len);
}

static bool solar_message_issue(uint8_t *data, uint16_t data_len)
{
    return slave_message_issue(DATA_FROM_SOLAR, data, data_len);
}

typedef struct
{
    bool (*func)(uint8_t *, uint16_t);
} node_consume_func_t;

node_consume_func_t node_consume_func[DATA_MARK_CNT] =
{
    [DATA_TO_MAIN] = { main_message_send },
    [DATA_TO_MACHINE] = { machine_message_send },
    [DATA_TO_POLE] = { pole_message_send },
    [DATA_TO_SOLAR] = { solar_message_send },
    [DATA_TO_MT80] = { mt80_message_send },
    [DATA_FROM_MAIN] = { main_message_issue },
    [DATA_FROM_MACHINE] = { machine_message_issue },
    [DATA_FROM_POLE] = { pole_message_issue },
    [DATA_FROM_SOLAR] = { solar_message_issue },
    [DATA_FROM_MT80] = { mt80_message_issue },
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
