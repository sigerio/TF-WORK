from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
STUBS = {
    "FreeRTOS.h": """#pragma once
#include <stdint.h>
#include <stdlib.h>
typedef unsigned TickType_t;
#define configTICK_RATE_HZ 1000U
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
    "task.h": "TickType_t xTaskGetTickCount(void);\nvoid vTaskDelay(unsigned ticks);\nvoid vTaskSuspendAll(void);\nint xTaskResumeAll(void);\n",
    "SEGGER_RTT.h": """#pragma once
typedef struct { unsigned RdOff, WrOff, SizeOfBuffer; } RTT_BUFFER;
typedef struct { RTT_BUFFER aUp[1]; } RTT_CONTROL;
extern RTT_CONTROL _SEGGER_RTT;
int SEGGER_RTT_printf(unsigned channel, const char *format, ...);
""",
    "uart_transport.h": """#pragma once
#define RX_TIMING_MARGIN_US 100U
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    BOARD_USART_MT80, BOARD_USART_MAIN, BOARD_USART_MACHINE, BOARD_USART_POLE, BOARD_USART_SOLAR, BOARD_USART_COUNT
} board_usart_id_e;
typedef struct { unsigned baudrate; } board_usart_config_t;
extern const board_usart_config_t board_usart_configs[BOARD_USART_COUNT];
uint16_t board_comm_time_us(void);
uint16_t board_rtu_gap_us(board_usart_id_e port);
bool board_usart_dma_rx_fetch(board_usart_id_e port, uint8_t *data, uint16_t *size);
bool board_usart_send(board_usart_id_e port, const uint8_t *data, uint16_t size);
""",
}
CHECK = r"""#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "comm_dispatch.c"
#include "comm_receive.c"

RTT_CONTROL _SEGGER_RTT = {{{0, 0, 1024}}};
static unsigned log_count;
static TickType_t mock_ticks;
TickType_t xTaskGetTickCount(void) { return mock_ticks; }
static unsigned scheduler_suspended;
static char last_log[1024];
void vTaskSuspendAll(void) { scheduler_suspended++; }
int xTaskResumeAll(void) { assert(scheduler_suspended == 1); scheduler_suspended--; return 0; }
int SEGGER_RTT_printf(unsigned channel, const char *format, ...) {
    va_list arguments;
    assert(channel == 0 && scheduler_suspended == 1);
    va_start(arguments, format);
    int length = vsnprintf(last_log, sizeof(last_log), format, arguments);
    va_end(arguments);
    assert(length > 0 && (unsigned)length < sizeof(last_log));
    log_count++;
    return length;
}

const board_usart_config_t board_usart_configs[BOARD_USART_COUNT] = {
    {115200}, {9600}, {9600}, {9600}, {9600}
};
static uint16_t mock_time;
uint16_t board_comm_time_us(void) { return mock_time; }
uint16_t board_rtu_gap_us(board_usart_id_e port) { (void)port; return 3646U + RX_TIMING_MARGIN_US; }
bool board_usart_dma_rx_fetch(board_usart_id_e port, uint8_t *data, uint16_t *size) {
    (void)port; (void)data; (void)size; return false;
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
    assert(data != NULL && size >= 4U && size <= 256U);
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
    mock_time = (uint16_t)(mock_time + board_rtu_gap_us(BOARD_USART_POLE));
    board_usart_rx_tick(mock_time);
}

static void check_receive(void) {
    unsigned previous_invalid = invalid_frames;
    uint8_t frame[] = {1, 3, 2, 0, 42, 0, 0};
    uint8_t received[256];
    modbus_rtu_append_crc(frame, 5);
    rx_frame_init();
    rx_channel_t *channel = rx_channel_find(BOARD_USART_POLE);
    mock_time = 65000;
    feed(BOARD_USART_POLE, frame, sizeof(frame));
    assert(rx_frame_read(channel, received) == 0);
    board_usart_rx_tick((uint16_t)(mock_time + channel->gap_us - 1U));
    assert(channel->count == 0);
    finish();
    assert(rx_frame_read(channel, received) == sizeof(frame));
    assert(memcmp(received, frame, sizeof(frame)) == 0);
    unsigned previous = queue_count;
    queue_spaces = 1;
    unsigned previous_waits = waits;
    unsigned previous_logs = log_count;
    rx_frame_submit(DATA_FROM_POLE, received, sizeof(frame));
    assert(log_count == previous_logs + 1 && strstr(last_log, "UART3 RX len=7"));
    assert(waits > previous_waits && queue_count == previous + 1);
    assert(queued.data_mark == DATA_FROM_POLE && memcmp(queued.data, frame, sizeof(frame)) == 0);
    vPortFree(queued.data);
    received[4] ^= 1;
    rx_frame_submit(DATA_FROM_POLE, received, sizeof(frame));
    assert(log_count == previous_logs + 1);
    assert(queue_count == previous + 1 && invalid_frames == previous_invalid + 1);

    for (unsigned index = 0; index < RX_FRAME_SLOTS + 1; index++) {
        feed(BOARD_USART_POLE, frame, sizeof(frame));
        finish();
    }
    assert(channel->count == RX_FRAME_SLOTS && channel->dropped == 1);
    for (unsigned index = 0; index < RX_FRAME_SLOTS; index++) {
        assert(rx_frame_read(channel, received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    for (unsigned index = 0; index < 180; index++) {
        feed(BOARD_USART_POLE, frame, sizeof(frame));
        finish();
        assert(rx_frame_read(channel, received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    memset(received, 0x55, sizeof(received));
    modbus_rtu_append_crc(received, 254);
    feed(BOARD_USART_POLE, received, sizeof(received));
    finish();
    assert(rx_frame_read(channel, received) == 256 && modbus_rtu_crc16(received, 256) == 0);
    feed(BOARD_USART_POLE, received, sizeof(received));
    feed(BOARD_USART_POLE, frame, 1);
    finish();
    assert(channel->count == 0 && channel->dropped == 2);
    feed(BOARD_USART_POLE, frame, 3);
    mock_time = (uint16_t)(mock_time + channel->byte_gap_us);
    feed(BOARD_USART_POLE, frame + 3, sizeof(frame) - 3);
    finish();
    assert(channel->count == 0 && channel->dropped == 3);
    feed(BOARD_USART_POLE, frame, sizeof(frame));
    board_usart_rx_error(BOARD_USART_POLE);
    finish();
    assert(channel->count == 0 && channel->dropped == 4);
    for (unsigned index = 0; index < sizeof(frame); index++) {
        mock_time = (uint16_t)(mock_time + 1042U);
        board_usart_rx_byte(BOARD_USART_POLE, frame[index]);
        board_usart_rx_byte(BOARD_USART_MACHINE, frame[index]);
        board_usart_rx_byte(BOARD_USART_SOLAR, frame[index]);
    }
    finish();
    for (unsigned index = 0; index < sizeof(channels) / sizeof(channels[0]); index++) {
        assert(rx_frame_read(&channels[index], received) == sizeof(frame));
        assert(memcmp(received, frame, sizeof(frame)) == 0);
    }
    for (unsigned index = 0; index < 5; index++) {
        feed(BOARD_USART_POLE, received, sizeof(received));
        finish();
    }
    assert(channel->count == 4 && channel->dropped == 5);
    for (unsigned index = 0; index < 4; index++) {
        assert(rx_frame_read(channel, received) == 256);
    }
    assert(allocations == 0);
}

static void check_logging(void) {
    uint8_t frame[MY_MESSAGE_DATA_SIZE] = {1, 3, 2, 0, 42};
    modbus_rtu_append_crc(frame, sizeof(frame) - 2);
    const DATA_MARK_E marks[] = {DATA_FROM_MT80, DATA_FROM_MAIN, DATA_FROM_MACHINE, DATA_FROM_POLE, DATA_FROM_SOLAR,
                                DATA_TO_MT80, DATA_TO_MAIN, DATA_TO_MACHINE, DATA_TO_POLE, DATA_TO_SOLAR};
    for (unsigned index = 0; index < sizeof(marks) / sizeof(marks[0]); index++) {
        char prefix[64];
        mock_ticks = index == 7 ? 0xffffffffU : 123456U + index;
        snprintf(prefix, sizeof(prefix), "[%u ms] [UART%u %s len=256] ", mock_ticks, index % 5, index < 5 ? "RX" : "TX");
        unsigned previous = log_count;
        comm_log_frame(marks[index], frame, sizeof(frame));
        assert(log_count == previous + 1 && strncmp(last_log, prefix, strlen(prefix)) == 0);
        assert(strlen(last_log) == strlen(prefix) + 3 * sizeof(frame) - 1 + 2);
        for (unsigned byte = 0; byte < sizeof(frame); byte++) {
            char expected[3];
            snprintf(expected, sizeof(expected), "%02X", frame[byte]);
            assert(strncmp(last_log + strlen(prefix) + 3 * byte, expected, 2) == 0);
        }
    }
    unsigned previous = log_count;
    frame[4] ^= 1;
    comm_log_frame(DATA_FROM_MAIN, frame, sizeof(frame));
    assert(log_count == previous);
    comm_log_frame(DATA_TO_MAIN, frame, sizeof(frame));
    assert(log_count == previous);
    for (unsigned length = 1; length < 4; length++) {
        comm_log_frame(DATA_FROM_MAIN, frame, length);
        comm_log_frame(DATA_TO_MAIN, frame, length);
    }
    assert(log_count == previous);
    previous = log_count;
    comm_log_frame(DATA_FROM_MACHINE, frame, sizeof(frame));
    comm_log_frame(DATA_TO_MACHINE, frame, sizeof(frame));
    comm_log_frame(DATA_FROM_MAIN, NULL, 8);
    comm_log_frame(DATA_FROM_MAIN, frame, 0);
    comm_log_frame(DATA_FROM_MAIN, frame, MY_MESSAGE_DATA_SIZE + 1);
    frame[4] ^= 1;
    comm_log_frame(DATA_MARK_NONE, frame, sizeof(frame));
    _SEGGER_RTT.aUp[0].WrOff = 1000;
    comm_log_frame(DATA_FROM_MAIN, frame, sizeof(frame));
    assert(log_count == previous);
    _SEGGER_RTT.aUp[0].RdOff = 900;
    _SEGGER_RTT.aUp[0].WrOff = 50;
    comm_log_frame(DATA_FROM_MAIN, frame, sizeof(frame));
    assert(log_count == previous + 1 && scheduler_suspended == 0);
    _SEGGER_RTT.aUp[0].RdOff = 0;
    _SEGGER_RTT.aUp[0].WrOff = 0;
}

static void check_whitelist(void) {
    uint8_t frame[] = {0, 6, 7, 0xD0, 0, 1, 0, 0};
    const DATA_MARK_E sources[] = {DATA_FROM_MAIN, DATA_FROM_MACHINE, DATA_FROM_POLE,
                                  DATA_FROM_SOLAR, DATA_FROM_MT80};
    for (unsigned source = 0; source < 5; ++source) {
        for (unsigned address = 0; address < 256; ++address) {
            bool allowed = source == 0 ? address >= 0x11 && address <= 0x14 :
                           address == (source == 4 ? 0x0A : 1);
            frame[0] = address;
            modbus_rtu_append_crc(frame, 6);
            unsigned before = queue_count, logs = log_count;
            rx_frame_submit(sources[source], frame, sizeof(frame));
            assert(queue_count == before + allowed && log_count == logs + allowed);
            assert(allocations == (unsigned)allowed);
            if (allowed) vPortFree(queued.data);
            else {
                assert(node_analyze((my_message_node_t){frame, sizeof(frame), sources[source]}));
                assert(queue_count == before);
            }
        }
    }
    /* A whole unknown-address frame must not expose a valid embedded request. */
    uint8_t outer[12] = {0xB4, 0x41, 0x11, 6, 7, 0xD0, 0, 1};
    modbus_rtu_append_crc(outer + 2, 6);
    modbus_rtu_append_crc(outer, 10);
    unsigned before = queue_count, logs = log_count;
    rx_frame_submit(DATA_FROM_MAIN, outer, sizeof(outer));
    assert(queue_count == before && log_count == logs && allocations == 0);
    frame[0] = 0x11;
    modbus_rtu_append_crc(frame, 6);
    frame[7] ^= 1;
    for (unsigned length = 0; length <= sizeof(frame); ++length) {
        rx_frame_submit(DATA_FROM_MAIN, frame, length);
        assert(queue_count == before && allocations == 0);
    }
}

static void check_payloads(void) {
    const uint8_t addresses[] = {0x11, 0x12, 0x13, 0x14};
    const DATA_MARK_E sources[] = {DATA_FROM_MACHINE, DATA_FROM_POLE, DATA_FROM_SOLAR, DATA_FROM_MT80};
    const DATA_MARK_E destinations[] = {DATA_TO_MACHINE, DATA_TO_POLE, DATA_TO_SOLAR, DATA_TO_MT80};
    const board_usart_id_e ports[] = {BOARD_USART_MACHINE, BOARD_USART_POLE, BOARD_USART_SOLAR, BOARD_USART_MT80};
    const uint8_t functions[] = {1, 3, 4, 5, 6, 0x10, 0x81, 0x83, 0x84, 0x85, 0x86, 0x90};
    const unsigned lengths[] = {5, 7, 8, 11, 15, 37, 41, 256};
    for (unsigned device = 0; device < 4; ++device) {
        uint8_t slave_address = device == 3 ? 0x0A : 1;
        check_route(addresses[device], DATA_FROM_MAIN, slave_address, destinations[device], ports[device]);
        check_route(slave_address, sources[device], addresses[device], DATA_TO_MAIN, BOARD_USART_MAIN);
        for (unsigned f = 0; f < sizeof(functions); ++f) {
            for (unsigned n = 0; n < sizeof(lengths) / sizeof(lengths[0]); ++n) {
                unsigned length = lengths[n];
                for (unsigned response = 0; response < 2; ++response) {
                    uint8_t frame[256];
                    for (unsigned i = 0; i < length; ++i) frame[i] = (uint8_t)(i * 7U + device);
                    frame[0] = response ? slave_address : addresses[device];
                    frame[1] = functions[f];
                    modbus_rtu_append_crc(frame, length - 2);
                    rx_frame_submit(response ? sources[device] : DATA_FROM_MAIN, frame, length);
                    my_message_node_t raw = queued;
                    assert(memcmp(raw.data, frame, length) == 0);
                    assert(node_analyze(raw));
                    vPortFree(raw.data);
                    assert(queued.data[0] == (response ? addresses[device] : slave_address));
                    assert(queued.data_mark == (response ? DATA_TO_MAIN : destinations[device]));
                    assert(queued.data_len == length && modbus_rtu_crc16(queued.data, length) == 0);
                    assert(memcmp(queued.data + 1, frame + 1, length - 3) == 0);
                    assert(node_analyze(queued));
                    assert(sent_port == (response ? BOARD_USART_MAIN : ports[device]));
                    vPortFree(queued.data);
                    assert(allocations == 0);
                }
            }
        }
    }
}

static void check_mt80_time_write(void) {
    /* 17:30:30: three registers in seconds/minutes/hours order. */
    uint8_t request[] = {0x14, 0x10, 0x96, 0x63, 0, 3, 6, 0, 0x1E, 0, 0x1E, 0, 0x11, 0, 0};
    uint8_t response[] = {0x0A, 0x10, 0x96, 0x63, 0, 3, 0, 0};
    uint8_t *frames[] = {request, response};
    const unsigned lengths[] = {sizeof(request), sizeof(response)};
    for (unsigned index = 0; index < 2; ++index) {
        modbus_rtu_append_crc(frames[index], lengths[index] - 2);
        unsigned before = queue_count;
        rx_frame_submit(index ? DATA_FROM_MT80 : DATA_FROM_MAIN, frames[index], lengths[index]);
        assert(queue_count == before + 1);
        my_message_node_t raw = queued;
        assert(node_analyze(raw));
        vPortFree(raw.data);
        assert(queued.data[0] == (index ? 0x14 : 0x0A));
        assert(queued.data_len == lengths[index]);
        assert(memcmp(queued.data + 1, frames[index] + 1, lengths[index] - 3) == 0);
        assert(modbus_rtu_crc16(queued.data, queued.data_len) == 0);
        assert(node_analyze(queued));
        assert(sent_port == (index ? BOARD_USART_MAIN : BOARD_USART_MT80));
        vPortFree(queued.data);
    }
    assert(allocations == 0);
}

int main(void) {
    check_logging();
    check_whitelist();
    check_payloads();
    check_mt80_time_write();
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
    uint8_t frame[] = {0x14, 3, 0, 0, 0, 1, 0, 0};
    modbus_rtu_append_crc(frame, 6);
    unsigned previous = queue_count;
    node_analyze((my_message_node_t){NULL, 8, DATA_FROM_MAIN});
    node_analyze((my_message_node_t){frame, 3, DATA_FROM_MAIN});
    node_analyze((my_message_node_t){frame, MY_MESSAGE_DATA_SIZE + 1, DATA_FROM_MAIN});
    queue_result = 0;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN}));
    queue_result = pdPASS;
    allocation_failed = true;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_FROM_MAIN}));
    assert(queue_count == previous && allocations == 0);
    allocation_failed = false;
    send_result = false;
    unsigned previous_logs = log_count;
    assert(!node_analyze((my_message_node_t){frame, sizeof(frame), DATA_TO_MAIN}));
    assert(log_count == previous_logs);
    send_result = true;
    assert(node_analyze((my_message_node_t){frame, sizeof(frame), DATA_TO_MAIN}));
    assert(log_count == previous_logs + 1 && strstr(last_log, "UART1 TX len=8"));
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
    subprocess.run(command, check=True)
    subprocess.run([str(directory / "check")], check=True, timeout=10)
    print("PASS: four routes, full address whitelist, payload/CRC, exceptions, queues and RX framing")
