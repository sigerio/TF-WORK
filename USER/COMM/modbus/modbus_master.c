#include "COMM/modbus/modbus_master.h"

#include <stdio.h>
#include <string.h>

#include "COMM/modbus/modbus_port.h"
#include "COMM/modbus/modbus_devices.h"
#include "COMM/modbus/devices/modbus_weather.h"
#include "DEBUG/debug_log.h"

/* 每条总线独立执行：出队发送 -> 等待响应 -> 解析或超时 -> 下一请求。 */
typedef enum {
    MODBUS_MASTER_IDLE = 0,      /**< 空闲，可发送下一帧                  */
    MODBUS_MASTER_WAIT_RSP,      /**< 等待从机响应中                      */
} modbus_master_state_t;

/** @brief 每条总线的主站运行时上下文 */
typedef struct {
    com_link_t link;
#if MODULE_MODBUS_ENABLE_DIAG
    modbus_diag_t diag;
#endif
    modbus_master_state_t state;
    modbus_rsp_expect_t rsp_expect;
    uint32_t wait_start_tick;
    uint32_t task_tick;
} modbus_master_bus_t;

static modbus_master_bus_t bus_masters[MODBUS_BUS_COUNT];

/* 只合并最新状态，不记录中间变位历史；发送中数据由 com_link 的 TX 缓冲独立持有。 */
static struct {
    uint32_t fault_value;
    uint32_t debug_mask;
    bool pending;
} weather_fault;

void modbus_master_queue_weather_fault(uint32_t fault_value)
{
    weather_fault.fault_value = fault_value;
    weather_fault.pending = true;
}

void modbus_master_repeat_weather_fault(void)
{
    weather_fault.pending = true;
}

void modbus_master_set_weather_fault_debug(uint32_t fault_mask)
{
    weather_fault.debug_mask = fault_mask;
    weather_fault.pending = true;
}

#ifdef DEBUG_ENABLE
static const char *modbus_bus_name(modbus_bus_t bus)
{
    return (bus == MODBUS_BUS_WEATHER) ? "WEATHER" : "AC";
}
#endif

static void modbus_log_frame(const char *direction, modbus_bus_t bus, const uint8_t *frame, uint32_t length)
{
#ifdef DEBUG_ENABLE
    char line[128];
    uint32_t offset = 0U;

    if ((direction == NULL) || (frame == NULL) || (length == 0U)) {
        return;
    }
    if ((bus == MODBUS_BUS_AC) && (debug_ctrl.bits.ac == 0U)) {
        return;
    }
    if ((bus == MODBUS_BUS_WEATHER) && (debug_ctrl.bits.weth == 0U)) {
        return;
    }

    /* 分段打印，避免长 Modbus 响应超过调试串口格式化缓冲区。 */
    while (offset < length) {
        int used = snprintf(line, sizeof(line), "MODBUS %s bus=%s len=%lu:", direction, modbus_bus_name(bus), (unsigned long)length);
        uint32_t count = 0U;

        if (used < 0) {
            return;
        }
        while ((count < 24U) && (offset < length) && ((uint32_t)used + 4U < sizeof(line))) {
            used += snprintf(&line[used], sizeof(line) - (size_t)used, " %02X", frame[offset++]);
            count++;
        }
        (void)snprintf(&line[used], sizeof(line) - (size_t)used, "\n");
        debug_printf(COLOR_RESET "%s[%-6s]" COLOR_RESET " %s", tag_color("APP"), "APP", line);
    }
#else
    (void)direction;
    (void)bus;
    (void)frame;
    (void)length;
#endif
}

#if MODULE_MODBUS_ENABLE_DIAG
#define MODBUS_DIAG_INC(bus_, field_) (bus_masters[bus_].diag.field_++)
#else
#define MODBUS_DIAG_INC(bus_, field_) ((void)0)
#endif

static void modbus_reset_runtime(void)
{
    modbus_txq_reset();
    weather_fault.fault_value = 0U;
    weather_fault.debug_mask = 0U;
    weather_fault.pending = false;
    for (uint32_t bus = 0U; bus < (uint32_t)MODBUS_BUS_COUNT; bus++) {
        modbus_master_bus_t *master = &bus_masters[bus];

        com_link_reset(&bus_masters[bus].link);
#if MODULE_MODBUS_ENABLE_DIAG
        memset(&master->diag, 0, sizeof(master->diag));
#endif
        memset(&master->rsp_expect, 0, sizeof(master->rsp_expect));
        master->state = MODBUS_MASTER_IDLE;
        master->wait_start_tick = 0U;
        master->task_tick = 0U;
    }
    modbus_devices_init();
}

static void modbus_wait_start(modbus_bus_t bus)
{
    modbus_master_bus_t *master = &bus_masters[bus];

    master->state = MODBUS_MASTER_WAIT_RSP;
    master->wait_start_tick = master->task_tick;
    MODBUS_DIAG_INC(bus, request_count);
}

static bool modbus_req_is_read(modbus_req_type_t request_type)
{
    return (request_type == MODBUS_REQ_READ_REGS) || (request_type == MODBUS_REQ_READ_BITS);
}

static bool modbus_req_is_write(modbus_req_type_t request_type)
{
    return (request_type == MODBUS_REQ_WRITE_COIL) || (request_type == MODBUS_REQ_WRITE_REG) || (request_type == MODBUS_REQ_WRITE_REGS);
}

bool modbus_master_init(void)
{
    for (uint32_t bus = 0U; bus < MODBUS_BUS_COUNT; bus++) {
        if (!modbus_port_open((modbus_bus_t)bus, &bus_masters[bus].link)) {
            return false;
        }
    }
    modbus_reset_runtime();
    return true;
}

#if MODULE_MODBUS_ENABLE_DIAG
void modbus_get_diag(modbus_diag_t *diag)
{
    modbus_txq_diag_t queue_diag;

    if (diag == NULL) {
        return;
    }

    memset(diag, 0, sizeof(*diag));
    for (uint32_t bus = 0U; bus < (uint32_t)MODBUS_BUS_COUNT; bus++) {
        diag->task_tick += bus_masters[bus].task_tick;
        diag->request_count += bus_masters[bus].diag.request_count;
        diag->response_count += bus_masters[bus].diag.response_count;
        diag->timeout_count += bus_masters[bus].diag.timeout_count;
        diag->crc_error_count += bus_masters[bus].diag.crc_error_count;
        diag->frame_error_count += bus_masters[bus].diag.frame_error_count;
        diag->unmatched_count += bus_masters[bus].diag.unmatched_count;
        diag->parse_ok_count += bus_masters[bus].diag.parse_ok_count;
        diag->parse_error_count += bus_masters[bus].diag.parse_error_count;
    }

    modbus_txq_get_diag(&queue_diag);
    diag->txq_write_pos = queue_diag.write_pos;
    diag->enqueue_count = queue_diag.enqueue_count;
    diag->dequeue_count = queue_diag.dequeue_count;
    diag->txq_overwrite_drop_count = queue_diag.overwrite_drop_count;
    diag->txq_full_drop_count = queue_diag.full_drop_count;
    diag->txq_invalid_count = queue_diag.invalid_count;
}

void modbus_clear_diag(void)
{
    for (uint32_t bus = 0U; bus < (uint32_t)MODBUS_BUS_COUNT; bus++) {
        memset(&bus_masters[bus].diag, 0, sizeof(bus_masters[bus].diag));
    }
    modbus_txq_clear_diag();
}
#endif

static bool modbus_receive_response(modbus_bus_t bus, uint8_t *frame, uint32_t *frame_len)
{
    modbus_master_bus_t *master = &bus_masters[bus];
    modbus_rsp_expect_t *expect = &master->rsp_expect;
    com_link_t *link = &bus_masters[bus].link;
    uint8_t head[3];
    uint8_t expect_function;
    uint8_t expect_byte_count;
    uint32_t available;
    uint32_t expected_len;
    uint32_t scan_budget;
    bool exception_response;
    bool read_request;
    bool write_request;

    if ((frame == NULL) || (frame_len == NULL) || (master->state != MODBUS_MASTER_WAIT_RSP)) {
        return false;
    }

    read_request = modbus_req_is_read(expect->req_type);
    write_request = modbus_req_is_write(expect->req_type);

    if ((expect->slave_addr == 0U) || (!read_request && !write_request)) {
        modbus_devices_set_online(expect->dev_type, false);
        MODBUS_DIAG_INC(bus, frame_error_count);
        master->state = MODBUS_MASTER_IDLE;
        return false;
    }

    expect_function = expect->rsp_func;
    expect_byte_count = expect->rsp_data_bytes;
    if (read_request) {
        if ((expect_byte_count == 0U) || (((uint32_t)expect_byte_count + 5U) > MODULE_MODBUS_RX_BUF_SIZE)) {
            modbus_devices_set_online(expect->dev_type, false);
            MODBUS_DIAG_INC(bus, frame_error_count);
            master->state = MODBUS_MASTER_IDLE;
            return false;
        }
        expected_len = (uint32_t)expect_byte_count + 5U;
    } else {
        expected_len = MODBUS_RTU_WRITE_SINGLE_RESPONSE_LEN;
    }
    scan_budget = com_link_rx_available(link);

    while (scan_budget >= 2U) {
        available = com_link_rx_available(link);
        if (available < 2U) {
            return false;
        }

        if (com_link_rx_peek(link, head, 2U) != 2U) {
            return false;
        }

        exception_response = (head[1] == (uint8_t)(expect_function | 0x80U));
        if ((head[0] != expect->slave_addr) || ((head[1] != expect_function) && !exception_response)) {
            if (com_link_rx_drop(link, 1U) != 1U) {
                return false;
            }
            MODBUS_DIAG_INC(bus, unmatched_count);
            scan_budget--;
            continue;
        }

        if (exception_response) {
            expected_len = MODBUS_RTU_EXCEPTION_LEN;
            available = com_link_rx_available(link);
            if (available < expected_len) {
                return false;
            }

            if (com_link_rx_peek(link, frame, (uint16_t)expected_len) != expected_len) {
                return false;
            }

            (void)com_link_rx_drop(link, (uint16_t)expected_len);
            *frame_len = expected_len;
            return true;
        }

        if (read_request) {
            available = com_link_rx_available(link);
            if (available < 3U) {
                return false;
            }

            if (com_link_rx_peek(link, head, 3U) != 3U) {
                return false;
            }

            if (head[2] != expect_byte_count) {
                (void)com_link_rx_drop(link, 3U);
                modbus_devices_set_online(expect->dev_type, false);
                MODBUS_DIAG_INC(bus, frame_error_count);
                master->state = MODBUS_MASTER_IDLE;
                return false;
            }
        }

        available = com_link_rx_available(link);
        if (available < expected_len) {
            return false;
        }

        if (com_link_rx_peek(link, frame, (uint16_t)expected_len) != expected_len) {
            return false;
        }

        (void)com_link_rx_drop(link, (uint16_t)expected_len);
        *frame_len = expected_len;
        return true;
    }

    return false;
}

static void modbus_count_parse_error(modbus_bus_t bus, modbus_rtu_status_t status)
{
    if (status == MODBUS_RTU_ERR_CRC) {
        MODBUS_DIAG_INC(bus, crc_error_count);
    } else {
        MODBUS_DIAG_INC(bus, parse_error_count);
    }
}

static void modbus_process_response(modbus_bus_t bus, const uint8_t *frame, uint32_t len)
{
    modbus_master_bus_t *master = &bus_masters[bus];
    modbus_rsp_expect_t *expect = &master->rsp_expect;
    uint8_t expect_function;
    modbus_rtu_status_t status;

    modbus_log_frame("RX", bus, frame, len);
    expect_function = expect->rsp_func;
    MODBUS_DIAG_INC(bus, response_count);
    if (frame[1] == (uint8_t)(expect_function | 0x80U)) {
        modbus_rtu_exception_t exception;

        status = modbus_rtu_parse_exception(frame, len, expect->slave_addr, expect_function, &exception);

        modbus_count_parse_error(bus, status);
        if ((status == MODBUS_RTU_OK) && modbus_req_is_write(expect->req_type)) {
            modbus_devices_set_online(expect->dev_type, true);
        } else {
            modbus_devices_set_online(expect->dev_type, false);
        }
        master->state = MODBUS_MASTER_IDLE;
        return;
    }

    status = modbus_devices_parse_response(expect, frame, len);
    if (status == MODBUS_RTU_OK) {
        modbus_devices_set_online(expect->dev_type, true);
        MODBUS_DIAG_INC(bus, parse_ok_count);
    } else {
        modbus_devices_set_online(expect->dev_type, false);
        modbus_count_parse_error(bus, status);
    }

    master->state = MODBUS_MASTER_IDLE;
}

static void modbus_receive_pending(modbus_bus_t bus)
{
    uint8_t frame[MODULE_MODBUS_RX_BUF_SIZE];
    uint32_t len = 0U;

    if (bus_masters[bus].state != MODBUS_MASTER_WAIT_RSP) {
        return;
    }

    if (modbus_receive_response(bus, frame, &len)) {
        modbus_process_response(bus, frame, len);
    }
}

static bool modbus_send_next(modbus_bus_t bus)
{
    modbus_master_bus_t *master = &bus_masters[bus];
    com_link_t *link = &bus_masters[bus].link;
    uint8_t *tx_buf = com_link_tx_buffer(link);
    modbus_rsp_expect_t expect;
    uint16_t frame_len = 0U;

    if (com_link_tx_busy(link) || (tx_buf == NULL)) {
        return false;
    }

    if (bus == MODBUS_BUS_WEATHER && weather_fault.pending) {
        frame_len = (uint16_t)modbus_weather_build_fault_notification(MODULE_MODBUS_WEATHER_ADDR,
            weather_fault.fault_value | weather_fault.debug_mask, tx_buf, MODULE_MODBUS_TX_BUF_SIZE);
        if (frame_len == 0U) return false;
        com_link_rx_clear(link);
        if (!com_link_tx_start(link, frame_len)) {
            MODBUS_DIAG_INC(bus, frame_error_count);
            return false; /* 保留待发状态，下次空闲重试，不消费普通请求。 */
        }
        weather_fault.pending = false;
        MODBUS_DIAG_INC(bus, request_count);
        modbus_log_frame("TX", bus, tx_buf, frame_len);
        /* 不进入 WAIT_RSP；tx_busy 在 DMA 和 USART 实际发送完成、方向恢复后才解除。 */
        return true;
    }

    if (!modbus_txq_pop(bus, tx_buf, MODULE_MODBUS_TX_BUF_SIZE, &frame_len, &expect)) {
        return false;
    }

    modbus_log_frame("TX", bus, tx_buf, frame_len);
    master->rsp_expect = expect;
    com_link_rx_clear(link);

    if (!com_link_tx_start(link, frame_len)) {
        MODBUS_DIAG_INC(bus, frame_error_count);
        return false;
    }

    modbus_wait_start(bus);
    return true;
}

static uint32_t modbus_response_timeout_ticks(void)
{
    uint32_t timeout_ms = (uint32_t)MODULE_MODBUS_RESPONSE_TIMEOUT_MS;
    uint32_t period_ms = (uint32_t)MODULE_MODBUS_MASTER_PERIOD_MS;
    uint32_t ticks;

    if (period_ms == 0U) {
        return 1U;
    }

    ticks = timeout_ms / period_ms;
    if ((ticks == 0U) || ((timeout_ms % period_ms) != 0U)) {
        ticks++;
    }

    return ticks;
}

void modbus_master_task(void)
{
    uint32_t timeout_ticks = modbus_response_timeout_ticks();

    for (uint32_t bus = 0U; bus < (uint32_t)MODBUS_BUS_COUNT; bus++) {
        modbus_master_bus_t *master = &bus_masters[bus];
        com_link_t *link = &bus_masters[bus].link;

        master->task_tick++;
        com_link_tx_service(link);
        modbus_receive_pending((modbus_bus_t)bus);

        if ((master->state == MODBUS_MASTER_WAIT_RSP) && ((master->task_tick - master->wait_start_tick) >= timeout_ticks)) {
            modbus_devices_set_online(master->rsp_expect.dev_type, false);
            master->state = MODBUS_MASTER_IDLE;
            MODBUS_DIAG_INC(bus, timeout_count);
        }

        if ((master->state == MODBUS_MASTER_IDLE) && !com_link_tx_busy(link)) {
            (void)modbus_send_next((modbus_bus_t)bus);
        }
    }
}

void modbus_weather_irq(void)
{
    com_link_irq(&bus_masters[MODBUS_BUS_WEATHER].link);
}

void modbus_ac_irq(void)
{
    com_link_irq(&bus_masters[MODBUS_BUS_AC].link);
}
