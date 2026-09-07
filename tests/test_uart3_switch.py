from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
STUBS = {
    "FreeRTOS.h": """#pragma once
#include <stdint.h>
#include <stdlib.h>
typedef unsigned TickType_t;
#define pdPASS 1
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(value) (value)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
void *pvPortMalloc(size_t size);
void vPortFree(void *pointer);
""",
    "queue.h": """#pragma once
typedef void *QueueHandle_t;
int xQueueSend(QueueHandle_t queue, const void *item, unsigned wait);
int xQueueReceive(QueueHandle_t queue, void *item, unsigned wait);
int xQueuePeek(QueueHandle_t queue, void *item, unsigned wait);
unsigned uxQueueSpacesAvailable(QueueHandle_t queue);
""",
    "task.h": "void vTaskDelay(unsigned ticks);\n",
    "uart_transport.h": """#pragma once
#define RX_TIMING_MARGIN_US 100U
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    BOARD_USART_TC, BOARD_USART_MAIN, BOARD_USART_WIND, BOARD_USART_RAIN, BOARD_USART_COUNT
} board_usart_id_e;
typedef struct { unsigned baudrate; } board_usart_config_t;
extern const board_usart_config_t board_usart_configs[BOARD_USART_COUNT];
uint16_t board_comm_time_us(void);
uint16_t board_rtu_gap_us(board_usart_id_e port);
bool board_usart_main_dma_rx_fetch(uint8_t *data, uint16_t *size);
bool board_usart_send(board_usart_id_e port, const uint8_t *data, uint16_t size);
""",
}
CHECK = r"""#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "comm_dispatch.c"
#include "comm_receive.c"

const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    {9600}, {9600}, {9600}, {9600}
};
static uint16_t mock_time;
uint16_t board_comm_time_us(void) { return mock_time; }
uint16_t board_rtu_gap_us(board_usart_id_e port) { (void)port; return 3646U + RX_TIMING_MARGIN_US; }
bool board_usart_main_dma_rx_fetch(uint8_t *data, uint16_t *size) {
    (void)data; (void)size; return false;
}

QueueHandle_t main_queue;
static my_message_node_t queued;
static unsigned queue_count, send_count, allocations;
static int queue_result = pdPASS;
static bool allocation_failed;
static board_usart_id_e sent_port;
static bool send_result = true;
static unsigned queue_spaces = 20;
static unsigned waits;
static bool queue_task_check;
static unsigned task_peeks;
static unsigned task_receives;
static my_message_node_t task_node;
static jmp_buf task_done;

void *pvPortMalloc(size_t size) {
    if (allocation_failed) return NULL;
    void *pointer = malloc(size);
    assert(pointer != NULL);
    allocations++;
    return pointer;
}
void vPortFree(void *pointer) { allocations--; free(pointer); }
int xQueueSend(QueueHandle_t queue, const void *item, unsigned wait) {
    (void)queue; (void)wait;
    if (queue_result == pdPASS) { queued = *(const my_message_node_t *)item; queue_count++; }
    return queue_result;
}
int xQueueReceive(QueueHandle_t queue, void *item, unsigned wait) {
    (void)queue; (void)wait;
    if (queue_task_check) {
        assert(task_peeks == 2 && task_receives == 0);
        *(my_message_node_t *)item = task_node;
        task_receives++;
        return pdPASS;
    }
    return 0;
}
int xQueuePeek(QueueHandle_t queue, void *item, unsigned wait) {
    (void)queue; (void)wait;
    if (queue_task_check) {
        if (task_receives != 0) longjmp(task_done, 1);
        *(my_message_node_t *)item = task_node;
        task_peeks++;
        return pdPASS;
    }
    return 0;
}
unsigned uxQueueSpacesAvailable(QueueHandle_t queue) { (void)queue; return queue_spaces; }
void vTaskDelay(unsigned ticks) {
    (void)ticks; waits++; queue_spaces = 20;
    if (queue_task_check) {
        assert(task_receives == 0 && allocations == 1);
        send_result = true;
    }
}
bool board_usart_send(board_usart_id_e port, const uint8_t *data, uint16_t size) {
    assert(data != NULL && size == 8U);
    if (!send_result) return false;
    sent_port = port; send_count++; return true;
}

static void check_route(uint8_t address, DATA_MARK_E source, uint8_t expected_address,
                        DATA_MARK_E destination, board_usart_id_e port) {
    uint8_t frame[] = {address, 3, 0, 0, 0, 1, 0, 0};
    modbus_rtu_append_crc(frame, 6);
    unsigned previous = queue_count;
    node_analyze((my_message_node_t){frame, sizeof(frame), source});
    assert(queue_count == previous + 1);
    assert(queued.data_mark == destination && queued.data_len == sizeof(frame));
    assert(queued.data[0] == expected_address && frame[0] == address);
    assert(memcmp(queued.data + 1, frame + 1, 5) == 0);
    assert(modbus_rtu_crc16(queued.data, queued.data_len) == 0);
    previous = send_count;
    node_analyze(queued);
    assert(send_count == previous + 1 && sent_port == port);
    vPortFree(queued.data);
    assert(allocations == 0);
}

static unsigned callback_count;
static uint8_t callback_data[] = {0x12, 0x34};

static bool check_callback(uint8_t *data, uint16_t size) {
    assert(data == callback_data && size == sizeof(callback_data));
    callback_count++;
    return false;
}

static void feed(board_usart_id_e port, const uint8_t *data, unsigned length) {
    for (unsigned index = 0; index < length; index++) {
        mock_time = (uint16_t)(mock_time + 1042U);
        board_usart_rx_byte(port, data[index]);
    }
}

static void finish(void) {
    mock_time = (uint16_t)(mock_time + board_rtu_gap_us(BOARD_USART_RAIN));
    board_usart_rx_tick(mock_time);
}

static void check_receive(void) {
    uint8_t frame[] = {1, 3, 2, 0, 42, 0, 0};
    uint8_t received[256];
    modbus_rtu_append_crc(frame, 5);
    rx_frame_init();
    rx_channel_t *channel = rx_channel_find(BOARD_USART_RAIN);
    mock_time = 65000;
    feed(BOARD_USART_RAIN, frame, sizeof(frame));
    assert(rx_frame_read(channel, received) == 0);
    board_usart_rx_tick((uint16_t)(mock_time + channel->gap_us - 1U));
    assert(channel->count == 0);
    finish();
    assert(rx_frame_read(channel, received) == sizeof(frame));
    assert(memcmp(received, frame, sizeof(frame)) == 0);
    unsigned previous = queue_count;
    queue_spaces = 1;
    unsigned previous_waits = waits;
    rx_frame_submit(UART3_RX_MARK, received, sizeof(frame));
    assert(waits > previous_waits && queue_count == previous + 1);
    assert(queued.data_mark == UART3_RX_MARK && memcmp(queued.data, frame, sizeof(frame)) == 0);
    vPortFree(queued.data);
    received[4] ^= 1;
    rx_frame_submit(UART3_RX_MARK, received, sizeof(frame));
    assert(queue_count == previous + 1 && invalid_frames == 1);

    for (unsigned index = 0; index < RX_FRAME_SLOTS + 1; index++) {
        feed(BOARD_USART_RAIN, frame, sizeof(frame));
        finish();
    }
    assert(channel->count == RX_FRAME_SLOTS && channel->dropped == 1);
    for (unsigned index = 0; index < RX_FRAME_SLOTS; index++) {
        assert(rx_frame_read(channel, received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    for (unsigned index = 0; index < 180; index++) {
        feed(BOARD_USART_RAIN, frame, sizeof(frame));
        finish();
        assert(rx_frame_read(channel, received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    memset(received, 0x55, sizeof(received));
    modbus_rtu_append_crc(received, 254);
    feed(BOARD_USART_RAIN, received, sizeof(received));
    finish();
    assert(rx_frame_read(channel, received) == 256 && modbus_rtu_crc16(received, 256) == 0);
    feed(BOARD_USART_RAIN, received, sizeof(received));
    feed(BOARD_USART_RAIN, frame, 1);
    finish();
    assert(channel->count == 0 && channel->dropped == 2);
    feed(BOARD_USART_RAIN, frame, 3);
    mock_time = (uint16_t)(mock_time + channel->byte_gap_us);
    feed(BOARD_USART_RAIN, frame + 3, sizeof(frame) - 3);
    finish();
    assert(channel->count == 0 && channel->dropped == 3);
    feed(BOARD_USART_RAIN, frame, sizeof(frame));
    board_usart_rx_error(BOARD_USART_RAIN);
    finish();
    assert(channel->count == 0 && channel->dropped == 4);
    for (unsigned index = 0; index < sizeof(frame); index++) {
        mock_time = (uint16_t)(mock_time + 1042U);
        board_usart_rx_byte(BOARD_USART_RAIN, frame[index]);
        board_usart_rx_byte(BOARD_USART_WIND, frame[index]);
        board_usart_rx_byte(BOARD_USART_TC, frame[index]);
    }
    finish();
    for (unsigned index = 0; index < sizeof(channels) / sizeof(channels[0]); index++) {
        assert(rx_frame_read(&channels[index], received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    for (unsigned index = 0; index < 5; index++) {
        feed(BOARD_USART_RAIN, received, sizeof(received));
        finish();
    }
    assert(channel->count == 4 && channel->dropped == 5);
    for (unsigned index = 0; index < 4; index++) {
        assert(rx_frame_read(channel, received) == 256);
    }
    assert(allocations == 0);
}

int main(void) {
    for (int mark = 0; mark < DATA_MARK_CNT; mark++) {
        node_consume_func_t saved = node_consume_func[mark];
        node_consume_func[mark].func = check_callback;
        assert(!node_analyze((my_message_node_t){callback_data, sizeof(callback_data), (DATA_MARK_E)mark}));
        assert(callback_count == (unsigned)mark + 1U);
        node_consume_func[mark].func = NULL;
        node_analyze((my_message_node_t){callback_data, sizeof(callback_data), (DATA_MARK_E)mark});
        assert(callback_count == (unsigned)mark + 1U);
        node_consume_func[mark] = saved;
    }
    node_analyze((my_message_node_t){callback_data, sizeof(callback_data), DATA_MARK_NONE});
    node_analyze((my_message_node_t){callback_data, sizeof(callback_data), DATA_MARK_CNT});
    assert(callback_count == DATA_MARK_CNT);
    assert(modbus_rtu_crc16((const uint8_t *)"123456789", 9) == 0x4B37);
    check_route(UART3_MASTER_ADDR, DATA_FROM_MAIN, 1, UART3_TX_MARK, BOARD_USART_RAIN);
    check_route(1, UART3_RX_MARK, UART3_MASTER_ADDR, DATA_TO_MAIN, BOARD_USART_MAIN);
    check_route(2, DATA_FROM_MAIN, 1, DATA_TO_WIND, BOARD_USART_WIND);
    check_route(1, DATA_FROM_WIND, 2, DATA_TO_MAIN, BOARD_USART_MAIN);
    check_route(5, DATA_FROM_MAIN, 1, DATA_TO_TC, BOARD_USART_TC);
    check_route(1, DATA_FROM_TC, 5, DATA_TO_MAIN, BOARD_USART_MAIN);

    uint8_t frame[] = {UART3_MASTER_ADDR == 3 ? 4 : 3, 3, 0, 0, 0, 1, 0, 0};
    modbus_rtu_append_crc(frame, 6);
    unsigned previous = queue_count;
    node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN});
    frame[0] = 1;
    modbus_rtu_append_crc(frame, 6);
    node_analyze((my_message_node_t){frame, sizeof(frame),
                 UART3_RX_MARK == DATA_FROM_RAIN ? DATA_FROM_WATER : DATA_FROM_RAIN});
    unsigned previous_send = send_count;
    message_send(UART3_TX_MARK == DATA_TO_RAIN ? DATA_TO_WATER : DATA_TO_RAIN, frame, sizeof(frame));
    assert(send_count == previous_send);
    frame[0] = 2;
    modbus_rtu_append_crc(frame, 6);
    node_analyze((my_message_node_t){frame, sizeof(frame), UART3_RX_MARK});
    frame[0] = UART3_MASTER_ADDR;
    modbus_rtu_append_crc(frame, 6);
    frame[7] ^= 1;
    node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN});
    frame[0] = 1;
    modbus_rtu_append_crc(frame, 6);
    frame[7] ^= 1;
    node_analyze((my_message_node_t){frame, sizeof(frame), UART3_RX_MARK});
    node_analyze((my_message_node_t){NULL, 8, DATA_FROM_MAIN});
    node_analyze((my_message_node_t){frame, 3, DATA_FROM_MAIN});
    frame[0] = UART3_MASTER_ADDR;
    modbus_rtu_append_crc(frame, 6);
    node_analyze((my_message_node_t){frame, MY_MESSAGE_DATA_SIZE + 1, DATA_FROM_MAIN});
    queue_result = 0;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN}));
    queue_result = pdPASS;
    allocation_failed = true;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN}));
    assert(queue_count == previous && allocations == 0);
    allocation_failed = false;
    send_result = false;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_TO_MAIN}));
    send_result = true;
    assert(node_analyze((my_message_node_t){frame, sizeof(frame), DATA_TO_MAIN}));
    check_receive();
    task_node = (my_message_node_t){pvPortMalloc(8), 8, DATA_TO_MAIN};
    memset(task_node.data, 0, 8);
    send_result = false;
    queue_task_check = true;
    if (setjmp(task_done) == 0) queue_pack_read(NULL);
    queue_task_check = false;
    assert(task_peeks == 2 && task_receives == 1 && allocations == 0);
    return 0;
}
"""


with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    for name, source in STUBS.items():
        (directory / name).write_text(source)
    (directory / "check.c").write_text(CHECK)
    command = [
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
        f"-I{directory}", f"-I{ROOT / 'Libraries/RingBuffer'}", f"-I{ROOT / 'Libraries/Modbus'}",
        f"-I{ROOT / 'Application/Communication'}", str(directory / "check.c"),
        str(ROOT / "Libraries/RingBuffer/ringbuf.c"),
        str(ROOT / "Libraries/Modbus/modbus_crc.c"),
        "-o", str(directory / "check"),
    ]
    for mode in (None, "UART3_DEVICE_RAIN", "UART3_DEVICE_WATER"):
        flags = [] if mode is None else [f"-DUART3_DEVICE={mode}"]
        subprocess.run(command + flags, check=True)
        subprocess.run([str(directory / "check")], check=True)
        print(f"PASS: {mode or 'default (water)'}")
    invalid = subprocess.run(command + ["-DUART3_DEVICE=2"], capture_output=True, text=True)
    assert invalid.returncode != 0 and "UART3_DEVICE must select" in invalid.stderr
    print("PASS: invalid configuration rejected")
