#include "COMM/modbus/modbus_devices.h"
/* Run with: python3 tests/check_app_safety.py. Hardware calls are stubbed; board, driver and control code are real. */
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "BOARD/board.h"
#include "CONTROL/motor/motor_control.h"
#include "CONTROL/motor/motor_driver.h"
#include "APP/app_status.h"
#include "gd32f10x_gpio.h"
#include "CONTROL/key/key_input.h"
#include "CONTROL/output/aux_output.h"
#include "COMM/iec104/app/iec104_app.h"
#include "COMM/iec104/app/iec104_dispatch.h"
#include "COMM/iec104/core/iec104_asdu.h"

static uint32_t low_inputs, configured_inputs;
static bool channel_enabled[2][4];
static uint16_t channel_pulse[2][4];
static bit_status lock_level;
static uint32_t aux_high_pins, aux_configured_pins;
static unsigned door_heater_reports;
static uint8_t reported_door_heater;
static unsigned drive_writes, fault_reports[2];
static uint8_t reported_fault[2];
static bool reject_report;
static xm8537_water_data_t water_cache;
static unsigned water_reports;
static uint8_t water_report_siq;
static uint8_t command_response_cot;

bool key_is_pressed_now(key_id_t key) { return false; }
uint8_t module_ws2812_effect_is_active(void) { return 0; }
bool modbus_ac_get(ac_device_t *out) { return false; }
bool modbus_xm7828_get(xm7828_th_data_t *out) { return false; }
bool modbus_xm8189_get(xm8189_wind_data_t *out) { return false; }
bool modbus_xm8537_get(xm8537_water_data_t *out) { *out = water_cache; return true; }
bool modbus_xm9505_get(xm9505_rain_data_t *out) { return false; }
int debug_printf(const char *format, ...) { return 0; }
bool modbus_ac_queue_write_status(uint8_t slave_addr, uint16_t ref, bool on) { assert(0); return false; }
bool modbus_ac_queue_write_param(uint8_t slave_addr, uint16_t ref, int16_t value) { assert(0); return false; }
bool modbus_xm7828_queue_write(uint8_t slave_addr, uint16_t ref, uint16_t value) { assert(0); return false; }
bool modbus_xm8189_queue_write(uint8_t slave_addr, uint16_t ref, uint16_t value) { assert(0); return false; }
uint8_t iec104_session_send_asdu(const uint8_t *asdu, uint8_t length, uint32_t now)
{
    if (asdu[0] == IEC104_TI_C_SC_NA_1) {
        assert(length == 10 && asdu[1] == 1);
        command_response_cot = asdu[2];
        return reject_report ? 0 : 1;
    }
    assert(length == 10 && asdu[0] == IEC104_TI_M_SP_NA_1 && asdu[1] == 1 && asdu[2] == IEC104_COT_SPONT);
    if (reject_report) return 0;
    uint32_t ioa = iec104_asdu_ioa_read(&asdu[6]);
    if (ioa == IEC104_IOA_YX_DOOR_LIMIT_FAULT || ioa == IEC104_IOA_YX_PLATFORM_LIMIT_FAULT) {
        unsigned index = ioa == IEC104_IOA_YX_PLATFORM_LIMIT_FAULT;
        reported_fault[index] = asdu[9];
        fault_reports[index]++;
    }
    if (ioa == 0x000016U) {
        door_heater_reports++;
        reported_door_heater = asdu[9];
    }
    if (ioa == 0x001021U) {
        water_reports++;
        water_report_siq = asdu[9];
    }
    return 1;
}

FlagStatus gpio_input_bit_get(uint32_t port, uint32_t pin)
{
    /* Any read of PA4/PA5 or PC6-PC9 fails this check. */
    assert(port == GPIOB && (pin & 0xF000U) == pin);
    return (low_inputs & pin) ? RESET : SET;
}
void gpio_init(uint32_t port, uint32_t mode, uint32_t speed, uint32_t pin)
{
    if (port == GPIOC) {
        assert(mode == GPIO_MODE_OUT_PP && (pin & (GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5)) == pin);
        aux_configured_pins |= pin;
    }
    if (mode == GPIO_MODE_IPU) {
        assert(port == GPIOB && (pin & 0xF000U) == pin);
        configured_inputs |= pin;
    }
}
void gpio_bit_write(uint32_t port, uint32_t pin, bit_status value)
{
    if (port == GPIOC) {
        assert(pin == GPIO_PIN_3 || pin == GPIO_PIN_4 || pin == GPIO_PIN_5);
        if (value == SET) aux_high_pins |= pin;
        else aux_high_pins &= ~pin;
        return;
    }
    assert(port == GPIOB && pin == GPIO_PIN_0);
    lock_level = value;
}
void gpio_pin_remap_config(uint32_t remap, ControlStatus value) {}
void rcu_periph_clock_enable(rcu_periph_enum periph) {}
void timer_deinit(uint32_t timer) {}
void timer_enable(uint32_t timer) {}
void timer_auto_reload_shadow_enable(uint32_t timer) {}
void timer_primary_output_config(uint32_t timer, ControlStatus value) {}
void timer_struct_para_init(timer_parameter_struct *parameters) { memset(parameters, 0, sizeof(*parameters)); }
void timer_init(uint32_t timer, timer_parameter_struct *parameters) {}
void timer_channel_output_struct_para_init(timer_oc_parameter_struct *parameters) { memset(parameters, 0, sizeof(*parameters)); }
void timer_channel_output_config(uint32_t timer, uint16_t channel, timer_oc_parameter_struct *parameters) {}
void timer_channel_output_mode_config(uint32_t timer, uint16_t channel, uint16_t mode) {}
void timer_channel_output_shadow_config(uint32_t timer, uint16_t channel, uint16_t shadow) {}
void timer_channel_output_pulse_value_config(uint32_t timer, uint16_t channel, uint16_t pulse)
{
    assert((timer == TIMER1 || timer == TIMER3) && channel < 4);
    channel_pulse[timer == TIMER3][channel] = pulse;
    if (pulse) drive_writes++;
}
void timer_channel_output_state_config(uint32_t timer, uint16_t channel, uint32_t state)
{
    assert((timer == TIMER1 || timer == TIMER3) && channel < 4);
    channel_enabled[timer == TIMER3][channel] = state == TIMER_CCX_ENABLE;
}
int8_t os_event_trigger(uint8_t event, const void *arg, uint16_t size, uint8_t count) { return 0; }

static void assert_stopped(bool door)
{
    for (unsigned channel = 0; channel < (door ? 4U : 2U); channel++) {
        assert(!channel_enabled[door][channel] && channel_pulse[door][channel] == 0);
    }
}

static void check_motion(uint32_t pin, void (*request)(void), bool door, unsigned expected_state)
{
    motor_control_status_t status;
    low_inputs = pin == GPIO_PIN_12 ? GPIO_PIN_15 : pin == GPIO_PIN_14 ? GPIO_PIN_13 : 0;
    motor_control_update();
    motor_control_set_estop(true);
    motor_control_set_estop(false);
    request();
    motor_control_update();
    motor_control_update();
    assert(channel_enabled[door][0] || channel_enabled[door][1]);
    if (door) assert(channel_enabled[1][2] || channel_enabled[1][3]);

    low_inputs |= pin;
    motor_control_update();
    assert_stopped(door);
    motor_control_get_status(&status);
    assert((unsigned)(door ? status.door_position : status.platform_position) == expected_state);
    assert(!is_signal_set(door ? OS_DOOR_S : OS_HELIPAD_S));
    if (pin == GPIO_PIN_14) assert(lock_level == RESET);

    /* A repeated request toward an already-active endpoint must not drive any channel. */
    request();
    motor_control_update();
    motor_control_update();
    assert_stopped(door);
}

static void tick(void)
{
    motor_control_update();
}

static void check_fault(unsigned combination, const char *phase)
{
    bool door_fault = (combination & 12U) == 12U;
    bool platform_fault = (combination & 3U) == 3U;
    bool boot = strcmp(phase, "boot") == 0;
    motor_control_status_t status;

    low_inputs = boot ? combination << 12 : 0;
    motor_control_init();
    if (boot) assert(drive_writes == 0); /* No startup brake pulse into a known fault. */
    iec104_dispatch_init();
    if (strcmp(phase, "moving") == 0 || strcmp(phase, "reversing") == 0) {
        motor_control_request_door_open();
        motor_control_request_platform_retract();
        tick();
        assert(channel_enabled[0][0] && channel_enabled[1][0] && channel_enabled[1][2]);
        if (strcmp(phase, "reversing") == 0) motor_control_request_door_close();
    }
    if (strcmp(phase, "pending") == 0) motor_control_request_platform_extend();
    low_inputs = combination << 12;
    unsigned writes_before = drive_writes;
    if (strcmp(phase, "command") == 0) {
        motor_control_request_platform_extend(); /* Must detect fault before the next periodic check. */
        assert_stopped(false);
        assert_stopped(true);
    }
    tick();
    assert(drive_writes == writes_before);
    assert_stopped(false);
    assert_stopped(true);
    motor_control_get_status(&status);
    assert(((status.door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) == door_fault && ((status.platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) == platform_fault);
    assert((status.door_fault != MOTOR_FAULT_NONE) == door_fault);
    assert((status.platform_fault != MOTOR_FAULT_NONE) == platform_fault);
    assert(!is_signal_set(OS_DOOR_S) && !is_signal_set(OS_HELIPAD_S));

    reject_report = true;
    iec104_dispatch_send_spontaneous(100);
    assert(fault_reports[0] == 0 && fault_reports[1] == 0);
    reject_report = false;
    iec104_dispatch_send_spontaneous(200);
    assert(reported_fault[0] == door_fault && reported_fault[1] == platform_fault);
    assert(fault_reports[0] == door_fault && fault_reports[1] == platform_fault);
    iec104_dispatch_send_spontaneous(300);
    assert(fault_reports[0] == door_fault && fault_reports[1] == platform_fault);
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measures[IEC104_APP_MEASURE_POINT_COUNT];
    iec104_app_collect_interrogation(points, measures);
    assert(points[34].value == door_fault && points[35].value == platform_fault);
    assert(points[5].value == 0 && points[6].value == 0);
    if (door_fault) assert(measures[0].value == 3.0F);
    if (platform_fault) assert(measures[1].value == 3.0F);

    motor_control_request_door_open();
    motor_control_request_door_close();
    motor_control_request_platform_extend();
    motor_control_request_platform_retract();
    motor_control_toggle_door();
    motor_control_toggle_platform();
    motor_control_request_door_open();
    motor_control_request_platform_retract();
    low_inputs = 0;
    for (unsigned i = 0; i < 20; i++) tick(); /* Outlast reversal delay: no old command may resume. */
    assert(drive_writes == writes_before);
    assert_stopped(false);
    assert_stopped(true);
    motor_control_get_status(&status);
    assert(!((status.door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) && !((status.platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0));
    assert(status.door_fault == MOTOR_FAULT_NONE && status.platform_fault == MOTOR_FAULT_NONE);
    iec104_dispatch_send_spontaneous(400);
    assert(reported_fault[0] == 0 && reported_fault[1] == 0);
    assert(fault_reports[0] == 2U * door_fault && fault_reports[1] == 2U * platform_fault);
    motor_control_request_door_open();
    motor_control_request_platform_retract();
    tick();
    assert(channel_enabled[0][0] && channel_enabled[1][0] && channel_enabled[1][2]);
}

static void reset_motion(uint32_t inputs)
{
    low_inputs = inputs;
    motor_control_update();
    motor_control_set_estop(true);
    motor_control_set_estop(false);
}

static void check_sequences(void)
{
    motor_control_status_t status;
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measures[IEC104_APP_MEASURE_POINT_COUNT];

    /* Extend request opens the door first; last close request replaces it and retracts the platform. */
    reset_motion(0);
    motor_control_request_platform_extend();
    tick();
    assert_stopped(false);
    assert(channel_enabled[1][0] && channel_enabled[1][2] && lock_level == SET);
    motor_control_request_door_close();
    assert_stopped(true);
    for (unsigned i = 0; i < 15; i++) tick();
    assert_stopped(true);
    assert(channel_enabled[0][0] && !channel_enabled[0][1]);
    low_inputs = GPIO_PIN_13;
    tick();
    assert_stopped(false);
    tick();
    assert(channel_enabled[1][1] && channel_enabled[1][3]);
    low_inputs |= GPIO_PIN_14;
    tick();
    assert_stopped(true);
    assert(lock_level == RESET);

    /* Opposite override: platform extension cancels pending close and waits for the shared door endpoint. */
    reset_motion(0);
    motor_control_request_door_close();
    tick();
    assert(channel_enabled[0][0]);
    motor_control_request_platform_extend();
    tick();
    assert(channel_enabled[1][0] && channel_enabled[1][2]);
    low_inputs = GPIO_PIN_15;
    for (unsigned i = 0; i < 12; i++) tick();
    assert_stopped(true);
    assert(channel_enabled[0][1] && !channel_enabled[0][0]);
    low_inputs |= GPIO_PIN_12;
    tick();
    assert_stopped(false);

    /* Position remains a physical endpoint while leaving it; telemetry must follow the actual action. */
    motor_control_request_platform_retract();
    tick();
    iec104_app_collect_interrogation(points, measures);
    assert(measures[1].value == 2.0F && points[2].value == 0 && points[6].value == 1);

    /* Repeated toggles during the reversal wait change the target without resetting the elapsed delay. */
    reset_motion(GPIO_PIN_13);
    motor_control_request_door_open();
    tick();
    motor_control_toggle_door();
    assert_stopped(true);
    for (unsigned i = 0; i < 4; i++) tick();
    motor_control_toggle_door();
    for (unsigned i = 0; i < 5; i++) tick();
    assert_stopped(true); /* 90 ms */
    tick();
    assert_stopped(true); /* 100 ms, start queued */
    tick();
    assert(channel_enabled[1][0] && channel_enabled[1][2]);
    motor_control_get_status(&status);
    assert(status.door_action == MOTOR_DOOR_OPENING);
    unsigned before = drive_writes;
    motor_control_request_door_open();
    tick();
    assert(drive_writes == before); /* Same-direction command is idempotent. */
    motor_control_set_estop(true);
    motor_control_set_estop(false);
    iec104_app_collect_interrogation(points, measures);
    assert(points[5].value == 0 && measures[0].value == 0.0F);

    /* Recovering only one conflicting pair must leave all outputs stopped until both recover and a new command arrives. */
    low_inputs = 0xF000U;
    tick();
    low_inputs = GPIO_PIN_12 | GPIO_PIN_13;
    tick();
    motor_control_get_status(&status);
    assert(status.door_fault == MOTOR_FAULT_NONE && status.platform_fault == MOTOR_FAULT_LIMIT_CONFLICT);
    motor_control_request_door_open();
    tick();
    assert_stopped(true);
    assert_stopped(false);
    low_inputs = 0;
    for (unsigned i = 0; i < 20; i++) tick();
    assert_stopped(true);
    assert_stopped(false);
}

static void check_driver_outputs(void)
{
    /* Driver takes individual electrical commands; no endpoint interpretation remains below control. */
    reset_motion(0);
    for (motor_id_e motor = MOTOR_PLATFORM; motor < MOTOR_ID_COUNT; motor++) {
        unsigned timer = motor != MOTOR_PLATFORM;
        unsigned backward = motor == MOTOR_DOOR2 ? 2U : 0U;
        unsigned forward = backward + 1U;
        assert(motor_driver_start(motor, MOTOR_DIR_FORWARD));
        assert(channel_enabled[timer][forward] && !channel_enabled[timer][backward]);
        assert(channel_pulse[timer][forward] == (board_pwm_configs[timer].period + 1U) * 99U / 100U);
        assert(motor_driver_get_direction(motor) == MOTOR_DIR_FORWARD);
        assert(motor_driver_start(motor, MOTOR_DIR_BACKWARD));
        assert(!channel_enabled[timer][forward] && channel_enabled[timer][backward]);
        assert(motor_driver_brake(motor));
        assert(channel_enabled[timer][forward] && channel_enabled[timer][backward]);
        assert(motor_driver_stop(motor));
        assert(!channel_enabled[timer][forward] && !channel_enabled[timer][backward]);
        assert(channel_pulse[timer][forward] == 0 && channel_pulse[timer][backward] == 0);
    }
    assert(!motor_driver_start((motor_id_e)-1, MOTOR_DIR_FORWARD));
    assert(!motor_driver_start(MOTOR_ID_COUNT, MOTOR_DIR_FORWARD));
    assert(!motor_driver_start(MOTOR_PLATFORM, (motor_direction_e)99));
}

static void check_door_heater_output(void)
{
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measures[IEC104_APP_MEASURE_POINT_COUNT];
    const uint32_t pins = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;

    aux_output_enable_door_heater();
    aux_output_disable_door_heater();
    aux_output_enable_indicator();
    aux_output_disable_indicator();
    aux_output_enable_interior_light();
    aux_output_disable_interior_light();
    assert(aux_configured_pins == 0 && aux_high_pins == 0); /* No drive before init. */
    aux_output_init();
    assert(aux_configured_pins == pins && aux_high_pins == pins); /* All outputs start off. */
    iec104_dispatch_init();
    for (unsigned enabled = 1; enabled <= 2; enabled++) {
        if (enabled == 1) aux_output_enable_door_heater();
        else aux_output_disable_door_heater();
        assert(((aux_high_pins & GPIO_PIN_5) == 0) == (enabled == 1));
        assert((aux_high_pins & (GPIO_PIN_3 | GPIO_PIN_4)) == (GPIO_PIN_3 | GPIO_PIN_4));
        iec104_app_collect_interrogation(points, measures);
        assert(points[10].ioa == 0x000016U && points[10].value == (enabled == 1) && points[10].quality == 0);
        iec104_dispatch_send_spontaneous(enabled * 100);
        assert(door_heater_reports == enabled && reported_door_heater == (enabled == 1));
    }
    aux_output_enable_indicator();
    aux_output_enable_interior_light();
    assert(aux_high_pins == GPIO_PIN_5);
    iec104_app_collect_interrogation(points, measures);
    assert(points[8].value == 1 && points[9].value == 1 && points[10].value == 0);
    aux_output_disable_indicator();
    aux_output_disable_interior_light();
    assert(aux_high_pins == pins);
    iec104_app_collect_interrogation(points, measures);
    assert(points[8].value == 0 && points[9].value == 0 && points[10].value == 0);
}

static void check_aux_remote_controls(void)
{
    const uint32_t commands[] = {0x000222U, 0x000223U, 0x000224U, 0x000225U};
    const uint32_t pins = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    iec104_frame_t command = {0};
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measures[IEC104_APP_MEASURE_POINT_COUNT];

    command.asdu_length = 10;
    command.asdu[0] = IEC104_TI_C_SC_NA_1;
    command.asdu[1] = 1;
    command.asdu[2] = IEC104_COT_ACT;
    command.asdu[4] = 1;
    iec104_dispatch_init();
    for (unsigned i = 0; i < 4; i++) {
        uint32_t before = aux_high_pins;
        uint32_t controlled_pin = (i < 2) ? GPIO_PIN_4 : GPIO_PIN_5;
        unsigned enabled = (i % 2 == 0);
        unsigned point = (i < 2) ? 9 : 10;

        assert(iec104_app_remote_control_is_known(commands[i]));
        iec104_asdu_ioa_write(&command.asdu[6], commands[i]);
        command.asdu[9] = 1; /* Execution without a selection must not drive an output. */
        iec104_dispatch_handle(&command, i * 100);
        assert(aux_high_pins == before && command_response_cot == (IEC104_COT_ACTTERM | IEC104_COT_NEGATIVE));
        command.asdu[9] = 0x81;
        iec104_dispatch_handle(&command, i * 100 + 10);
        assert(aux_high_pins == before && command_response_cot == IEC104_COT_ACTCON);
        command.asdu[9] = 1;
        iec104_dispatch_handle(&command, i * 100 + 20);
        assert(command_response_cot == IEC104_COT_ACTTERM);
        assert(aux_high_pins == (enabled ? (pins & ~controlled_pin) : pins));
        iec104_app_collect_interrogation(points, measures);
        assert(points[point].value == enabled && points[point].quality == 0);
        unsigned reports_before = door_heater_reports;
        iec104_dispatch_send_spontaneous(i * 100 + 30);
        if (i >= 2) assert(door_heater_reports == reports_before + 1 && reported_door_heater == enabled);
    }
    /* Both enable and disable IOAs use SCO=1; SCO=0 is only acknowledged. */
    iec104_asdu_ioa_write(&command.asdu[6], 0x000224U);
    command.asdu[9] = 0x80;
    iec104_dispatch_handle(&command, 500);
    command.asdu[9] = 0;
    iec104_dispatch_handle(&command, 510);
    assert(aux_high_pins == pins && command_response_cot == IEC104_COT_ACTTERM);
    /* If the execution response cannot be queued, the output must stay off. */
    command.asdu[9] = 0x81;
    iec104_dispatch_handle(&command, 520);
    reject_report = true;
    command.asdu[9] = 1;
    iec104_dispatch_handle(&command, 530);
    reject_report = false;
    assert(aux_high_pins == pins);
}

static void check_water_values(void)
{
    uint8_t frame[7] = {4, 3, 2};
    uint8_t ack[8];
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measures[IEC104_APP_MEASURE_POINT_COUNT];

    for (uint32_t value = 0; value <= 0xFFFFU; value++) {
        modbus_rtu_put_u16_be(&frame[3], (uint16_t)value);
        modbus_rtu_append_crc(frame, 5);
        assert(modbus_xm8537_parse_measurement(frame, sizeof(frame), 4, &water_cache) == MODBUS_RTU_OK);
        assert(water_cache.status.online && water_cache.status.sensor_fault == (value > 1));
        assert(water_cache.raw == (value == 0 ? 0 : 1) && water_cache.water_detected == (value != 0));
        if (value == 1) iec104_dispatch_init();
    }
    iec104_app_collect_interrogation(points, measures);
    assert(points[15].value == 1 && points[15].quality == 0); /* Invalid data is still online. */
    assert(points[16].value == 1 && points[16].quality == 0xA0U);
    assert(measures[6].value == 1.0F && measures[6].quality == points[16].quality);
    iec104_dispatch_send_spontaneous(100);
    assert(water_reports == 1 && water_report_siq == 0xA1U);

    assert(modbus_rtu_build_write_single_request(4, 6, XUNCHIP_REG_COMM_MODE, 1, ack, sizeof(ack)) == 8);
    assert(modbus_xm8537_parse_write_ack(ack, sizeof(ack), 4, 6, XUNCHIP_REG_COMM_MODE, 1, &water_cache) == MODBUS_RTU_OK);
    assert(water_cache.status.sensor_fault); /* Configuration acknowledgement cannot validate the measurement. */

    modbus_rtu_put_u16_be(&frame[3], 0);
    modbus_rtu_append_crc(frame, 5);
    assert(modbus_xm8537_parse_measurement(frame, sizeof(frame), 4, &water_cache) == MODBUS_RTU_OK);
    assert(!water_cache.status.sensor_fault && !water_cache.water_detected && water_cache.raw == 0);
    iec104_dispatch_send_spontaneous(200);
    assert(water_reports == 2 && water_report_siq == 0);
    modbus_rtu_put_u16_be(&frame[3], 2);
    modbus_rtu_append_crc(frame, 5);
    assert(modbus_xm8537_parse_measurement(frame, sizeof(frame), 4, &water_cache) == MODBUS_RTU_OK);
    assert(water_cache.status.sensor_fault && !water_cache.water_detected && water_cache.raw == 0);
}

int main(int argc, char **argv)
{
    if (argc == 3) {
        check_fault((unsigned)atoi(argv[1]), argv[2]);
        return 0;
    }
    motor_control_init();
    assert(configured_inputs == 0xF000U);
    check_door_heater_output();
    check_aux_remote_controls();
    check_water_values();

    /* Read and interpret all 16 combinations through the control layer, including idle changes. */
    for (unsigned combination = 0; combination < 16; combination++) {
        motor_control_status_t status;
        low_inputs = combination << 12;
        motor_control_update();
        motor_control_get_status(&status);
        assert(((status.door_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) == ((combination & 12U) == 12U));
        assert(((status.platform_fault & MOTOR_FAULT_LIMIT_CONFLICT) != 0) == ((combination & 3U) == 3U));
        assert(status.door_position == ((combination & 12U) == 4U ? MOTOR_DOOR_CLOSED :
            (combination & 12U) == 8U ? MOTOR_DOOR_OPENED : MOTOR_DOOR_INTERMEDIATE));
        assert(status.platform_position == ((combination & 3U) == 1U ? MOTOR_PLATFORM_EXTENDED :
            (combination & 3U) == 2U ? MOTOR_PLATFORM_RETRACTED : MOTOR_PLATFORM_INTERMEDIATE));
    }
    low_inputs = 0;
    motor_control_update();

    check_motion(GPIO_PIN_12, motor_control_request_platform_extend, false, MOTOR_PLATFORM_EXTENDED);
    check_motion(GPIO_PIN_13, motor_control_request_platform_retract, false, MOTOR_PLATFORM_RETRACTED);
    check_motion(GPIO_PIN_14, motor_control_request_door_close, true, MOTOR_DOOR_CLOSED);
    check_motion(GPIO_PIN_15, motor_control_request_door_open, true, MOTOR_DOOR_OPENED);

    check_sequences();
    check_driver_outputs();
    low_inputs = 0;
    motor_control_request_door_close();
    motor_control_request_platform_extend();
    motor_control_update();
    low_inputs = 0xF000U;
    motor_control_update();
    motor_control_status_t status;
    motor_control_get_status(&status);
    assert(status.door_fault != MOTOR_FAULT_NONE && status.platform_fault != MOTOR_FAULT_NONE);
    assert_stopped(true);
    assert_stopped(false);
    return 0;
}
