/* Real protocol, device management, queue and master; only the byte link is simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "COMM/modbus/modbus_master.h"
#include "COMM/modbus/modbus_devices.h"
#include "COMM/modbus/modbus_port.h"
#include "COMM/modbus/devices/modbus_weather.h"
#include "APP/app_status.h"

static com_link_resource_t resources[MODBUS_BUS_COUNT];
static ringbuf_t receive_fifo[MODBUS_BUS_COUNT];
static uint8_t receive_bytes[MODBUS_BUS_COUNT][MODULE_MODBUS_RINGBUF_SIZE];
static uint8_t transmit_bytes[MODBUS_BUS_COUNT][MODULE_MODBUS_TX_BUF_SIZE];
static uint8_t sent_frames[64][MODULE_MODBUS_TX_BUF_SIZE];
static unsigned sent_ticks[64], sent_buses[64], sent_count, tick;
static uint16_t sent_lengths[64];
static bool hold_weather_tx, fail_weather_tx;

bool modbus_port_open(modbus_bus_t bus, com_link_t *link)
{
    resources[bus].rx_fifo = &receive_fifo[bus];
    resources[bus].tx_dma_buf = transmit_bytes[bus];
    ringbuf_init(&receive_fifo[bus], receive_bytes[bus], sizeof(receive_bytes[bus]));
    link->res = &resources[bus];
    return true;
}

void com_link_reset(com_link_t *link) { link->tx_busy = false; ringbuf_reset(link->res->rx_fifo); }
uint8_t *com_link_tx_buffer(com_link_t *link) { return link->res->tx_dma_buf; }
bool com_link_tx_busy(const com_link_t *link) { return link->tx_busy; }
bool com_link_tx_start(com_link_t *link, uint16_t len)
{
    if (link->res == &resources[MODBUS_BUS_WEATHER] && fail_weather_tx) return false;
    assert(!link->tx_busy);
    assert(sent_count < 64 && len <= MODULE_MODBUS_TX_BUF_SIZE);
    memcpy(sent_frames[sent_count], link->res->tx_dma_buf, len);
    sent_lengths[sent_count] = len;
    sent_buses[sent_count] = (unsigned)(link->res - resources);
    sent_ticks[sent_count++] = tick;
    link->tx_busy = true;
    return true;
}
void com_link_tx_service(com_link_t *link)
{
    if (link->res != &resources[MODBUS_BUS_WEATHER] || !hold_weather_tx) link->tx_busy = false;
}
uint16_t com_link_rx_available(com_link_t *link) { return ringbuf_available(link->res->rx_fifo); }
uint16_t com_link_rx_peek(com_link_t *link, uint8_t *data, uint16_t len) { return ringbuf_peek(link->res->rx_fifo, data, len); }
uint16_t com_link_rx_drop(com_link_t *link, uint16_t len) { return ringbuf_drop(link->res->rx_fifo, len); }
void com_link_rx_clear(com_link_t *link) { ringbuf_reset(link->res->rx_fifo); }

static void setup(void)
{
    assert(modbus_master_init());
    tick = sent_count = 0;
    hold_weather_tx = fail_weather_tx = false;
}
static void step(void) { tick++; modbus_master_task(); }
static void inject(modbus_bus_t bus, const uint8_t *frame, uint16_t len)
{
    assert(ringbuf_write(&receive_fifo[bus], frame, len) == len);
}
static uint16_t response_for(unsigned sent, uint8_t *response)
{
    const uint8_t *request = sent_frames[sent];
    uint16_t quantity = modbus_rtu_get_u16_be(&request[4]);
    uint16_t length;

    response[0] = request[0];
    response[1] = request[1];
    if ((request[1] == 5) || (request[1] == 6)) {
        memcpy(response, request, 8);
        return 8;
    }
    response[2] = (uint8_t)((request[1] == 1) ? (quantity + 7) / 8 : quantity * 2);
    memset(response + 3, 0, response[2]);
    length = response[2] + 5;
    modbus_rtu_append_crc(response, length - 2);
    return length;
}

static void check_poll_plan(void)
{
    const uint8_t addresses[] = {5, 2, 4, 3, 5, 2, 1, 1, 1, 1, 0xB4,
        0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14};
    const uint16_t starts[] = {0, 0, 0, 0, 0x6B, 0x6B, AC_PARAM_SEG_MEASURE_START, AC_PARAM_SEG_HUMIDITY_START,
        AC_PARAM_SEG_CONTROL_START, AC_STATUS_SEG_ALL_START, 0,
        0x3100, 0x3110, 0x311D, 0x3200, 0x3205, 0x3300, 0x330F, 0x3400, 0x3410, 0x3420, 0, 3};
    const uint16_t quantities[] = {2, 2, 1, 6, 2, 2, AC_PARAM_SEG_MEASURE_COUNT, AC_PARAM_SEG_HUMIDITY_COUNT,
        AC_PARAM_SEG_CONTROL_COUNT, AC_STATUS_SEG_ALL_COUNT, 0, 16, 11, 5, 4, 1, 15, 6, 16, 16, 4, 1, 1};
    const modbus_dev_type_t devices[] = {MODBUS_DEV_XM7828_TH, MODBUS_DEV_XM8189_WIND, MODBUS_DEV_XM8537_WATER,
        MODBUS_DEV_XM9505_RAIN, MODBUS_DEV_XM7828_TH, MODBUS_DEV_XM8189_WIND, MODBUS_DEV_AC, MODBUS_DEV_AC, MODBUS_DEV_AC, MODBUS_DEV_AC};
    uint8_t frame[8], expected[8];
    uint16_t len;
    modbus_rsp_expect_t expect;

    setup();
    const unsigned order[] = {0,11,1,12,2,13,3,14,4,15,5,16,6,7,8,9,10,0,17,1,18,2,19,3,20,4,21,5,22};
    unsigned last_weather_address = 0;
    for (unsigned n = 0; n < 58; n++) {
        unsigned i = order[n % 29];
        if (i == 10) {
            modbus_devices_poll();
            assert(modbus_txq_count() == 0);
            unsigned before = sent_count;
            step();
            assert(sent_count == before + 1 && sent_frames[before][0] == 0xB4);
            continue;
        }
        modbus_bus_t bus = (i < 6 || i > 10) ? MODBUS_BUS_WEATHER : MODBUS_BUS_AC;
        uint8_t function = (i == 9 || i >= 21) ? 1 : ((i > 10) ? 4 : 3);
        uint16_t base = (i < 6 || i > 10) ? 0 : ((i == 9) ? AC_STATUS_READ_BASE_REF : AC_PARAM_READ_BASE_REF);

        modbus_devices_poll();
        assert(modbus_txq_count() == 1);
        assert(modbus_txq_pop(bus, frame, sizeof(frame), &len, &expect));
        assert(modbus_rtu_build_read_request(addresses[i], function, starts[i] - base, quantities[i], expected, sizeof(expected)) == 8);
        assert(len == 8 && memcmp(frame, expected, 8) == 0);
        assert(expect.bus == bus && expect.dev_type == ((i > 10) ? MODBUS_DEV_TIS : devices[i]) && expect.slave_addr == addresses[i]);
        assert(expect.start_addr == starts[i] && expect.quantity == quantities[i] && expect.rsp_func == function);
        assert(expect.req_type == ((function == 1) ? MODBUS_REQ_READ_BITS : MODBUS_REQ_READ_REGS));
        assert(expect.rsp_data_bytes == ((function == 1) ? (quantities[i] + 7) / 8 : quantities[i] * 2));
        if (bus == MODBUS_BUS_WEATHER) {
            assert(last_weather_address != addresses[i]);
            last_weather_address = addresses[i];
        }
    }
}

static void check_transactions(void)
{
    uint8_t response[256];
    xm7828_th_data_t th;
    xm8189_wind_data_t wind;
    xm8537_water_data_t water;
    xm9505_rain_data_t rain;
    ac_device_t ac;
    setup();
    unsigned last_weather_address = 0;
    for (unsigned i = 0; i < 58; i++) {
        modbus_devices_poll();
        unsigned sent = sent_count;
        step();
        assert(sent_count == sent + 1);
        if (sent_frames[sent][0] != 0xB4) {
            if (sent_buses[sent] == MODBUS_BUS_WEATHER) {
                assert(last_weather_address != sent_frames[sent][0]);
                last_weather_address = sent_frames[sent][0];
            }
            uint16_t len = response_for(sent, response);
            inject((modbus_bus_t)sent_buses[sent], response, len);
        }
        step();
    }
    assert(sent_count == 58 && modbus_txq_count() == 0);
    assert(modbus_xm7828_get(&th) && th.status.online && th.calibration_valid == 3);
    assert(modbus_xm8189_get(&wind) && wind.status.online && wind.calibration_valid == 3);
    assert(modbus_xm8537_get(&water) && water.status.online && !water.status.sensor_fault);
    assert(modbus_xm9505_get(&rain) && rain.status.online);
    assert(modbus_ac_get(&ac) && !ac.status.sensor_fault && ac.params.marks.raw && ac.run_status.marks.raw);

    setup();
    modbus_devices_poll();
    modbus_devices_poll();
    step();
    uint16_t length = response_for(0, response);
    inject(MODBUS_BUS_WEATHER, response, 4);
    step();
    assert(sent_count == 1); /* A partial response must not release the next request. */
    inject(MODBUS_BUS_WEATHER, response + 4, length - 4);
    step();
    assert(sent_count == 2 && sent_frames[1][0] == 0x14);
    for (unsigned i = 0; i < 20; i++) step();
    tis_device_t tis;
    assert(modbus_tis_get(&tis) && !tis.status.online);
#if MODULE_MODBUS_ENABLE_DIAG
    modbus_diag_t diag;
    modbus_get_diag(&diag);
    assert(diag.request_count == 2 && diag.parse_ok_count == 1 && diag.timeout_count == 1);
#endif

    setup();
    unsigned replied = 0;
    for (unsigned i = 1; i <= 151; i++) {
        if (replied < sent_count) {
            uint16_t len = response_for(replied, response);
            inject((modbus_bus_t)sent_buses[replied], response, len);
            replied++;
        }
        step();
        if (i % 15 == 0) modbus_devices_poll();
    }
    assert(sent_count == 10);
    for (unsigned i = 1; i < 10; i++) assert(sent_ticks[i] - sent_ticks[i - 1] == 15);
}

static void check_writes_and_faults(void)
{
    uint8_t response[256];
    xm8537_water_data_t water;
    setup();
    assert(modbus_xm7828_queue_write(5, XM7828_REG_TEMP_CALIBRATION, (uint16_t)-100));
    assert(modbus_xm8189_queue_write(2, XM8189_REG_SPEED_CALIBRATION, 100));
    assert(modbus_xm8537_queue_write(4, XUNCHIP_REG_COMM_MODE, 1));
    assert(modbus_xm9505_queue_write(3, XUNCHIP_REG_COMM_MODE, 1));
    assert(modbus_ac_queue_write_status(1, AC_STATUS_POWER_MONITOR, true));
    assert(modbus_ac_queue_write_param(1, AC_PARAM_REG_COOLING_START, 250));
    assert(modbus_txq_count() == 6);
    assert(!modbus_xm8537_queue_write(4, XUNCHIP_REG_COMM_MODE, 3));
    assert(!modbus_xm7828_queue_write(5, XUNCHIP_REG_DEVICE_ADDR, 7));
    assert(!modbus_ac_queue_write_param(1, AC_PARAM_REG_DEVICE_ADDR, 2));
    unsigned handled = 0;
    step();
    while (handled < 6) {
        unsigned available = sent_count;
        while (handled < available) {
            uint16_t len = response_for(handled, response);
            assert(modbus_rtu_check_crc(sent_frames[handled], 8));
            inject((modbus_bus_t)sent_buses[handled++], response, len);
        }
        step();
    }
    assert(sent_frames[1][1] == 5 && modbus_rtu_get_u16_be(&sent_frames[1][4]) == 0xFF00);

    setup();
    for (unsigned i = 0; i < 5; i++) modbus_devices_poll();
    /* Reach the water measurement and return an illegal value. */
    step();
    for (unsigned i = 0; i < 4; i++) {
        uint16_t len = response_for(i, response);
        inject(MODBUS_BUS_WEATHER, response, len);
        step();
    }
    assert(sent_frames[4][0] == 4);
    response_for(4, response);
    response[4] = 2;
    modbus_rtu_append_crc(response, 5);
    inject(MODBUS_BUS_WEATHER, response, 7);
    step();
    assert(modbus_xm8537_get(&water) && water.status.online && water.status.sensor_fault);
    assert(modbus_xm8537_queue_write(4, XUNCHIP_REG_COMM_MODE, 1));
    step();
    inject(MODBUS_BUS_WEATHER, sent_frames[5], 8);
    step();
    assert(modbus_xm8537_get(&water) && water.status.sensor_fault);
}

static void check_protocol_builders(void)
{
    uint8_t frame[8];
    const uint8_t rain_request[] = {3, 3, 0, 0, 0, 6, 0xC4, 0x2A};
    setup();
    assert(modbus_xm9505_build_read_request(3, 0, 6, frame, sizeof(frame)) == 8);
    assert(memcmp(frame, rain_request, 8) == 0 && modbus_txq_count() == 0);
    assert(modbus_xm9505_build_read_request(3, 0, 6, frame, 7) == 0);
    assert(modbus_xm9505_build_read_request(0, 0, 6, frame, 8) == 0);
    assert(modbus_xm9505_build_read_request(3, 0, 6, NULL, 8) == 0);
    assert(modbus_xm9505_build_read_request(3, 0xFFFF, 2, frame, 8) == 0);
    assert(modbus_xm8537_build_write_request(4, XUNCHIP_REG_COMM_MODE, 1, frame, 8) == 8);
    assert(modbus_txq_count() == 0);
    assert(modbus_ac_build_read_params_request(1, AC_PARAM_REG_RESERVED_30027, 1, frame, 8) == 0);
    assert(modbus_ac_build_read_status_request(1, AC_STATUS_READ_BASE_REF - 1, 1, frame, 8) == 0);
}

static void assert_weather_fault(unsigned index, uint32_t value)
{
    assert(sent_buses[index] == MODBUS_BUS_WEATHER && sent_lengths[index] == 9U);
    assert(sent_frames[index][0] == 0xB4 && sent_frames[index][1] == 0x41 && sent_frames[index][2] == 4U);
    for (unsigned i = 0; i < 4; i++) assert(sent_frames[index][3 + i] == (uint8_t)(value >> (i * 8)));
    assert(modbus_rtu_check_crc(sent_frames[index], 9));
}

static void check_weather_fault_priority(void)
{
    uint8_t response[256];
    uint8_t frame[10];
    const uint8_t prefix[] = {0xB4, 0x41, 0x04, 0x78, 0x56, 0x34, 0x12};
    memset(frame, 0xA5, sizeof(frame));
    assert(modbus_weather_build_fault_notification(0xB4, 0x12345678, frame, sizeof(frame)) == 9);
    assert(memcmp(frame, prefix, sizeof(prefix)) == 0 && frame[9] == 0xA5);
    assert(frame[7] == 0x7F && frame[8] == 0xA7); /* Independent known CRC vector. */
    assert(modbus_rtu_check_crc(frame, 9));
    assert(modbus_weather_build_fault_notification(0, 0, frame, sizeof(frame)) == 0);
    assert(modbus_weather_build_fault_notification(248, 0, frame, sizeof(frame)) == 0);
    assert(modbus_weather_build_fault_notification(0xB4, 0, NULL, 9) == 0);
    assert(modbus_weather_build_fault_notification(0xB4, 0, frame, 8) == 0);
    assert(memcmp(frame, prefix, sizeof(prefix)) == 0);

    setup();
    modbus_devices_poll();
    modbus_devices_poll();
    step(); /* 05 is in flight, TIS is queued. */
    modbus_master_queue_weather_fault(1);
    modbus_master_queue_weather_fault(0x01020304);
    modbus_master_repeat_weather_fault();
    assert(modbus_ac_queue_write_status(1, AC_STATUS_POWER_MONITOR, true));
    step();
    assert(sent_count == 2 && sent_buses[1] == MODBUS_BUS_AC); /* AC progresses, WEATHER waits. */
    assert(modbus_txq_count() == 1);
    uint16_t length = response_for(0, response);
    inject(MODBUS_BUS_WEATHER, response, length);
    hold_weather_tx = true;
    step();
    assert(sent_count == 3);
    assert_weather_fault(2, 0x01020304);
    assert(modbus_txq_count() == 1); /* B4 precedes queued TIS without removing it. */

    modbus_master_queue_weather_fault(0xFFFFFFFF);
    modbus_master_queue_weather_fault(0); /* Recovery replaces pending alarm; it cannot alter in-flight data. */
    for (unsigned i = 0; i < 12; i++) step();
    assert(sent_count == 3);
    assert(memcmp(transmit_bytes[MODBUS_BUS_WEATHER], sent_frames[2], 9) == 0);
    hold_weather_tx = false;
    step();
    assert_weather_fault(3, 0);
    step();
    assert(sent_count == 5 && sent_frames[4][0] == 0x14);
    assert(sent_ticks[4] == sent_ticks[3] + 1); /* No response timeout wait for B4. */

    setup();
    modbus_devices_poll();
    modbus_master_queue_weather_fault(1);
    fail_weather_tx = true;
    step();
    assert(sent_count == 0 && modbus_txq_count() == 1);
    modbus_master_queue_weather_fault(8);
    fail_weather_tx = false;
    step();
    assert_weather_fault(0, 8);
    assert(modbus_txq_count() == 1);
    step();
    assert(sent_count == 2 && sent_frames[1][0] == 5);

    setup();
    modbus_master_queue_weather_fault(0x01FFFFFF);
    step();
    for (unsigned i = 0; i < 30; i++) step();
    assert(sent_count == 1 && get_signal_state().byte == 0); /* No B4 online signal or timeout. */
#if MODULE_MODBUS_ENABLE_DIAG
    modbus_diag_t diag;
    modbus_get_diag(&diag);
    assert(diag.request_count == 1 && diag.timeout_count == 0 && diag.response_count == 0);
#endif
    /* Periodic repeat sends latest retained value, no duplicate normal-queue frame. */
    for (unsigned i = 0; i < 17; i++) modbus_devices_poll();
    step();
    assert_weather_fault(1, 0x01FFFFFF);

    setup();
    for (unsigned i = 0; i < MODULE_MODBUS_SEND_QUEUE_MAX; i++) assert(modbus_xm8537_queue_write(4, XUNCHIP_REG_COMM_MODE, 1));
    modbus_master_queue_weather_fault(2);
    step();
    assert_weather_fault(0, 2);
    assert(modbus_txq_count() == MODULE_MODBUS_SEND_QUEUE_MAX); /* Dedicated slot cannot be evicted by queue fullness. */

    setup();
    modbus_devices_poll();
    step();
    modbus_master_queue_weather_fault(4);
    for (unsigned i = 0; i < 19; i++) step();
    assert(sent_count == 1);
    step();
    assert_weather_fault(1, 4); /* Also sends after the current sensor times out. */

    setup();
    modbus_master_queue_weather_fault(0x00014004); /* Real platform fault and offline statuses. */
    modbus_master_set_weather_fault_debug(1);
    step();
    assert_weather_fault(0, 0x00014005);
    hold_weather_tx = true;
    modbus_master_set_weather_fault_debug(2); /* Change simulation without altering in-flight bytes. */
    step();
    assert(sent_count == 1 && memcmp(transmit_bytes[MODBUS_BUS_WEATHER], sent_frames[0], 9) == 0);
    hold_weather_tx = false;
    step();
    assert_weather_fault(1, 0x00014006);
    modbus_master_set_weather_fault_debug(8);
    step();
    assert_weather_fault(2, 0x0001400C);
    modbus_master_repeat_weather_fault();
    step();
    assert_weather_fault(3, 0x0001400C); /* Periodic sync preserves the selected simulation. */
    modbus_master_queue_weather_fault(0x00014000); /* Real recovery cannot erase the simulation. */
    step();
    assert_weather_fault(4, 0x00014008);
    modbus_master_set_weather_fault_debug(0);
    step();
    assert_weather_fault(5, 0x00014000); /* WSE:0 restores true state, not unconditional zero. */
    modbus_master_repeat_weather_fault();
    step();
    assert_weather_fault(6, 0x00014000);
}

/* New devices share the real master/queue; no motion or factory reset runs on hardware. */
static void check_wanrong_commands(void)
{
    const modbus_dev_type_t devices[] = {MODBUS_DEV_MACHINE_EXTEND, MODBUS_DEV_POLE_RAISE, MODBUS_DEV_SOLAR_UNFOLD};
    const uint8_t addresses[] = {0x11, 0x12, 0x13};
    uint8_t frame[8], response[8];
    uint16_t length;
    modbus_rsp_expect_t expect;

    setup();
    for (unsigned device = 0; device < 3; device++) {
        for (unsigned command = 0; command < 3; command++) {
            assert(modbus_wanrong_queue_command(devices[device], (wanrong_command_e)command));
            assert(modbus_txq_pop(MODBUS_BUS_WEATHER, frame, sizeof(frame), &length, &expect));
            assert(length == 8 && frame[0] == addresses[device] && frame[1] == 6);
            assert(frame[2] == 0x07 && frame[3] == 0xD0 && frame[4] == 0 && frame[5] == command);
            assert(modbus_rtu_check_crc(frame, 8) && expect.dev_type == devices[device]);
            assert(modbus_devices_parse_response(&expect, frame, 8) == MODBUS_RTU_OK);
            memcpy(response, frame, 8);
            response[5] = (uint8_t)((command + 1) % 3);
            modbus_rtu_append_crc(response, 6);
            assert(modbus_devices_parse_response(&expect, response, 8) == MODBUS_RTU_ERR_VALUE);
            memcpy(response, frame, 8);
            response[3]++;
            modbus_rtu_append_crc(response, 6);
            assert(modbus_devices_parse_response(&expect, response, 8) == MODBUS_RTU_ERR_VALUE);
            assert(modbus_devices_parse_response(&expect, frame, 7) == MODBUS_RTU_ERR_LEN);
            frame[7] ^= 1;
            assert(modbus_devices_parse_response(&expect, frame, 8) == MODBUS_RTU_ERR_CRC);
        }
    }
    assert(!modbus_wanrong_queue_command(MODBUS_DEV_TIS, WANRONG_STOP));
    assert(!modbus_wanrong_queue_command(devices[0], (wanrong_command_e)-1));
    assert(!modbus_wanrong_queue_command(devices[0], (wanrong_command_e)3));
    assert(modbus_txq_count() == 0);
    assert(modbus_wanrong_build_command(0, WANRONG_STOP, frame, 8) == 0);
    assert(modbus_wanrong_build_command(248, WANRONG_STOP, frame, 8) == 0);
    assert(modbus_wanrong_build_command(0x11, WANRONG_STOP, frame, 7) == 0);

    /* Timeout belongs to one command; subsequent instances still send/complete. */
    for (unsigned i = 0; i < 3; i++) assert(modbus_wanrong_queue_command(devices[i], WANRONG_STOP));
    step();
    for (unsigned i = 0; i < 19; i++) step();
    assert(sent_count == 1);
    step();
    assert(sent_count == 2 && sent_frames[1][0] == 0x12);
    uint8_t exception[] = {0x12, 0x86, 2, 0, 0};
    modbus_rtu_append_crc(exception, 3);
    inject(MODBUS_BUS_WEATHER, exception, 5);
    step();
    assert(sent_count == 3 && sent_frames[2][0] == 0x13);
    inject(MODBUS_BUS_WEATHER, sent_frames[2], 8);
    step();
    assert(modbus_txq_count() == 0 && get_signal_state().byte == 0);
#if MODULE_MODBUS_ENABLE_DIAG
    modbus_diag_t diag;
    modbus_get_diag(&diag);
    assert(diag.timeout_count == 1 && diag.parse_ok_count == 1);
#endif
}

static void check_tis_reads(void)
{
    /* Independent review of all table ranges, including configuration and holes. */
    const uint16_t starts[] = {0x3002, 0x3100, 0x3110, 0x311D, 0x3200, 0x3205, 0x3300, 0x330F,
        0x3400, 0x3410, 0x3420, 0x9000, 0x9010, 0x9020, 0x902C, 0x9038, 0x9045, 0x9049};
    const uint16_t counts[] = {12,16,11,5,4,1,15,6,16,16,4,16,16,12,12,12,3,3};
    uint8_t response[64];
    tis_device_t data, before;
    unsigned total = 0;
    setup();
    for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
        uint8_t function = starts[i] >= 0x9000 ? 3 : 4;
        assert(modbus_tis_queue_read(function, starts[i], counts[i]));
        step();
        unsigned sent = sent_count - 1;
        assert(sent_buses[sent] == MODBUS_BUS_WEATHER && sent_frames[sent][0] == 0x14);
        assert(sent_frames[sent][1] == function && modbus_rtu_get_u16_be(sent_frames[sent] + 2) == starts[i]);
        uint16_t len = response_for(sent, response);
        for (unsigned j = 0; j < counts[i]; j++) {
            modbus_rtu_put_u16_be(response + 3 + j * 2, (uint16_t)(0x8000U | ((starts[i] + j) & 0x0FFFU)));
        }
        modbus_rtu_append_crc(response, len - 2);
        inject(MODBUS_BUS_WEATHER, response, 4);
        step();
        inject(MODBUS_BUS_WEATHER, response + 4, len - 4);
        step();
        total += counts[i];
    }
    assert(total == 180 && modbus_tis_get(&data) && data.status.online);
    assert(data.rated_pv_power == 0x80058004UL);
    assert(data.pv1_input_power == 0x81038102UL && data.pv2_output_power == 0x810F810EUL);
    assert(data.battery_voltage == 0x8114 && data.battery_temperature == -32488);
    assert(data.year_generation == 0x8310830FUL && data.learned_capacity == 0x83148313UL);
    assert(data.bms_power == 0x84048403UL && data.bms_current == 0x8402 && data.bms_group_current == 0x841B);
    assert(data.bms_max_cell_temperature == -31736);
    assert(data.low_charge_temperature == -32745 && data.low_discharge_temperature == -32744);
    assert(data.load_mode == 0x8022 && data.low_soc == 0x803D && data.manual_equalize == 0x804B);
    assert((data.device_state & TIS_DEVICE_DUPLICATE_PARALLEL_ID) != 0);
    assert(!modbus_tis_get(NULL));

    /* Negative/positive extremes and signed-before-scaling at a declared int16 field. */
    const uint16_t raw[] = {0xFF9C, 0xFC18, 0x8000, 0x7FFF, 0, 2500};
    const int32_t signed_values[] = {-100, -1000, -32768, 32767, 0, 2500};
    for (unsigned i = 0; i < 6; i++) {
        response[0] = 0x14; response[1] = 4; response[2] = 2;
        modbus_rtu_put_u16_be(response + 3, raw[i]);
        modbus_rtu_append_crc(response, 5);
        assert(modbus_tis_parse_read(response, 7, 0x14, 4, 0x3118, 1, &data) == MODBUS_RTU_OK);
        assert(data.battery_temperature == signed_values[i]);
    }
    before = data;
    response[6] ^= 1;
    assert(modbus_tis_parse_read(response, 7, 0x14, 4, 0x3118, 1, &data) == MODBUS_RTU_ERR_CRC);
    assert(memcmp(&before, &data, sizeof(data)) == 0);
    response[0] = 0x15;
    modbus_rtu_append_crc(response, 5);
    assert(modbus_tis_parse_read(response, 7, 0x14, 4, 0x3118, 1, &data) == MODBUS_RTU_ERR_ADDR);
    response[0] = 0x14;
    modbus_rtu_append_crc(response, 5);
    assert(modbus_tis_parse_read(response, 7, 0x14, 4, 0x3118, 2, &data) == MODBUS_RTU_ERR_BYTE_COUNT);
    assert(memcmp(&before, &data, sizeof(data)) == 0);

    assert(!modbus_tis_queue_read(4, 0x3100, 0));
    assert(!modbus_tis_queue_read(4, 0x3100, 17));
    assert(!modbus_tis_queue_read(4, 0xFFFF, 2));
    assert(!modbus_tis_queue_read(4, 0x311A, 4));
    assert(!modbus_tis_queue_read(4, 0x3200, 6));
    assert(!modbus_tis_queue_read(3, 0x9043, 3));
    assert(!modbus_tis_queue_read(3, 0x9047, 3));
    assert(!modbus_tis_queue_read(4, 0x3102, 1));
    assert(!modbus_tis_queue_read(4, 0x3103, 1));
    assert(!modbus_tis_queue_read(3, 0x9019, 2));
    assert(!modbus_tis_queue_read(1, 0, 4));
    assert(!modbus_tis_queue_read(6, 0x9000, 1));
    assert(modbus_txq_count() == 0);

    const uint16_t coils[] = {0, 3, 7, 8, 9};
    setup();
    for (unsigned i = 0; i < 5; i++) {
        assert(modbus_tis_queue_read(1, coils[i], 1));
        step();
        uint16_t len = response_for(i, response);
        response[3] = 1;
        modbus_rtu_append_crc(response, len - 2);
        inject(MODBUS_BUS_WEATHER, response, len);
        step();
    }
    assert(modbus_tis_get(&data) && data.charging_enabled && data.manual_output_enabled);
    assert(data.factory_reset && data.clear_energy && data.fault_reset);
}

static void check_tis_writes(void)
{
    uint8_t frame[64], response[8];
    uint16_t values[17] = {0};
    uint16_t length;
    modbus_rsp_expect_t expect;
    tis_device_t data;
    setup();
    values[0] = (uint16_t)-100;
    assert(modbus_tis_queue_write_params(TIS_REG_LOW_CHARGE_TEMPERATURE, values, 1, 0));
    assert(modbus_txq_pop(MODBUS_BUS_WEATHER, frame, sizeof(frame), &length, &expect));
    const uint8_t prefix[] = {0x14, 0x10, 0x90, 0x17, 0, 1, 2, 0xFF, 0x9C};
    assert(length == 11 && memcmp(frame, prefix, sizeof(prefix)) == 0 && modbus_rtu_check_crc(frame, length));
    assert(expect.req_type == MODBUS_REQ_WRITE_REGS && expect.quantity == 1 && expect.echo_addr == 0x9017);
    memcpy(response, frame, 6);
    modbus_rtu_append_crc(response, 6);
    assert(modbus_devices_parse_response(&expect, response, 8) == MODBUS_RTU_OK);
    response[5] = 2;
    modbus_rtu_append_crc(response, 6);
    assert(modbus_devices_parse_response(&expect, response, 8) == MODBUS_RTU_ERR_VALUE);
    response[5] = 1; response[3]++;
    modbus_rtu_append_crc(response, 6);
    assert(modbus_devices_parse_response(&expect, response, 8) == MODBUS_RTU_ERR_VALUE);
    assert(modbus_tis_get(&data) && data.low_charge_temperature == 0); /* Ack never fabricates readback. */

    /* 16 valid parameters, maximum frame; per-value validation applies to every element. */
    const uint16_t params[] = {1, 100, 300, 1, 10, 10, 1, 1400, 1400, 1300, 1400, 1400, 1350, 1300, 1200, 1200};
    memcpy(values, params, sizeof(params));
    assert(modbus_tis_build_write_params(0x14, 0x9000, values, 16, 0, frame, sizeof(frame)) == 41);
    assert(frame[6] == 32 && modbus_rtu_check_crc(frame, 41));
    assert(modbus_tis_build_write_params(0x14, 0x9000, values, 16, 0, frame, 40) == 0);
    assert(!modbus_tis_queue_write_params(0x9000, values, 17, 1));
    assert(!modbus_tis_queue_write_params(0x9000, values, 0, 1));
    assert(!modbus_tis_queue_write_params(0x9000, NULL, 1, 1));
    assert(!modbus_tis_queue_write_params(0x3100, values, 1, 1));
    assert(!modbus_tis_queue_write_params(0x9043, values, 3, 1));
    values[0] = 2000;
    assert(!modbus_tis_queue_write_params(0x9007, values, 1, 1));
    values[0] = 1400;
    assert(!modbus_tis_queue_write_params(0x9007, values, 1, 0));
    assert(modbus_tis_queue_write_params(0x9007, values, 1, 1));
    modbus_txq_reset();
    values[0] = (uint16_t)-2501;
    assert(!modbus_tis_queue_write_params(0x9017, values, 1, 0));
    values[0] = 4;
    assert(!modbus_tis_queue_write_params(0x9022, values, 1, 0));
    assert(!modbus_tis_queue_write_coil(1, true));
    assert(modbus_txq_count() == 0);
    values[0] = 0x0C;
    assert(modbus_tis_build_write_params(0x14, 0x9000, values, 1, 0, frame, sizeof(frame)) == 11);
    values[0] = 0x0D;
    assert(!modbus_tis_queue_write_params(0x9000, values, 1, 0));
    values[0] = 0x0E;
    assert(!modbus_tis_queue_write_params(0x9000, values, 1, 0));

    /* All table-defined coils can be built; actual destructive commands are not sent to hardware. */
    const uint16_t coils[] = {0,3,7,8,9};
    for (unsigned i = 0; i < 5; i++) {
        assert(modbus_tis_queue_write_coil(coils[i], true));
        assert(modbus_txq_pop(MODBUS_BUS_WEATHER, frame, sizeof(frame), &length, &expect));
        assert(frame[1] == 5 && frame[4] == 0xFF && frame[5] == 0);
        assert(modbus_devices_parse_response(&expect, frame, length) == MODBUS_RTU_OK);
    }
    values[0] = 8;
    assert(modbus_tis_queue_write_params(0x903D, values, 1, 0));
    step();
    memcpy(response, sent_frames[0], 6);
    modbus_rtu_append_crc(response, 6);
    inject(MODBUS_BUS_WEATHER, response, 8);
    step();
    assert(modbus_tis_get(&data) && data.status.online);
    assert(modbus_tis_queue_write_params(0x903D, values, 1, 0));
    step();
    uint8_t exception[] = {0x14, 0x90, 3, 0, 0};
    modbus_rtu_append_crc(exception, 3);
    inject(MODBUS_BUS_WEATHER, exception, 5);
    step();
    assert(modbus_tis_get(&data) && data.status.online && data.low_soc == 0);
    assert(modbus_tis_queue_read(4, 0x3114, 1));
    step();
    for (unsigned i = 0; i < 19; i++) step();
    assert(modbus_tis_get(&data) && data.status.online);
    step();
    assert(modbus_tis_get(&data) && !data.status.online);
}

int main(void)
{
    check_poll_plan();
    check_transactions();
    check_writes_and_faults();
    check_protocol_builders();
    check_weather_fault_priority();
    check_wanrong_commands();
    check_tis_reads();
    check_tis_writes();
    puts("PASS: Wanrong commands, TIS full table/short frames/signed values/write echoes, B4 priority/latest-state/no-reply/DMA ownership/retry, Modbus poll order, response routing, independent buses, partial RX, timing and water faults");
    return 0;
}
