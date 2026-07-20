#include "iec104_dispatch.h"

#include <stddef.h>
#include <string.h>

#include "../core/iec104_asdu.h"
#include "../core/iec104_session.h"
#include "iec104_app.h"

#define IEC104_COMMON_ADDRESS 0x0001U

typedef struct {
    uint32_t ioa;
    uint8_t value;
    uint8_t selected;
} iec104_single_command_t;

typedef struct {
    uint32_t ioa;
    uint32_t value;
    uint8_t value_length;
    uint8_t selected;
} iec104_tuning_point_t;

typedef struct {
    int8_t point_index;
    uint32_t ioa;
    uint32_t value;
    uint16_t register_value;
    uint8_t pi;
    uint8_t value_length;
} iec104_tuning_object_t;

static iec104_single_command_t selected_command;
static iec104_tuning_point_t tuning_points[IEC104_APP_TUNING_POINT_COUNT];
static iec104_status_point_t previous_status_points[IEC104_APP_STATUS_POINT_COUNT];

static uint8_t iec104_dispatch_send(uint8_t type, uint8_t vsq, uint8_t cot, const uint8_t *objects, uint8_t objects_length, uint32_t now)
{
    uint8_t asdu[IEC104_MAX_ASDU_LENGTH];
    uint8_t asdu_length;

    asdu_length = iec104_asdu_build(type, vsq, cot, IEC104_COMMON_ADDRESS, objects, objects_length, asdu, sizeof(asdu));
    if (asdu_length == 0U) {
        return 0U;
    }

    return iec104_session_send_asdu(asdu, asdu_length, now);
}

static uint8_t iec104_send_command_response(const iec104_frame_t *frame, uint8_t cot, uint32_t now)
{
    return iec104_dispatch_send(frame->asdu[0], frame->asdu[1], cot, &frame->asdu[IEC104_ASDU_HEADER_LENGTH], (uint8_t)(frame->asdu_length - IEC104_ASDU_HEADER_LENGTH), now);
}

static uint8_t iec104_asdu_header_is_valid(const iec104_frame_t *frame, uint8_t type, uint8_t cot, uint32_t now)
{
    uint16_t common_address;

    if (frame->asdu_length < IEC104_ASDU_HEADER_LENGTH) {
        return 0U;
    }

    if (frame->asdu[0] != type) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_TYPE, now);
        return 0U;
    }

    common_address = iec104_asdu_u16_read(&frame->asdu[4]);
    if (common_address != IEC104_COMMON_ADDRESS) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_CA, now);
        return 0U;
    }

    if (frame->asdu[2] != cot) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_COT, now);
        return 0U;
    }

    return 1U;
}

static uint8_t iec104_send_status_points(const iec104_status_point_t *points, uint8_t cot, uint32_t now)
{
    uint8_t objects[IEC104_APP_STATUS_POINT_COUNT * 4U];
    uint8_t i;

    for (i = 0U; i < IEC104_APP_STATUS_POINT_COUNT; i++) {
        iec104_asdu_ioa_write(&objects[i * 4U], points[i].ioa);
        objects[i * 4U + 3U] = points[i].value | points[i].quality;
    }

    return iec104_dispatch_send(IEC104_TI_M_SP_NA_1, IEC104_APP_STATUS_POINT_COUNT, cot, objects, sizeof(objects), now);
}

static uint8_t iec104_send_measure_points(const iec104_measure_point_t *points, uint8_t cot, uint32_t now)
{
    uint8_t objects[IEC104_APP_MEASURE_POINT_COUNT * 8U];
    uint8_t i;

    for (i = 0U; i < IEC104_APP_MEASURE_POINT_COUNT; i++) {
        iec104_asdu_ioa_write(&objects[i * 8U], points[i].ioa);
        memcpy(&objects[i * 8U + 3U], &points[i].value, sizeof(points[i].value));
        objects[i * 8U + 7U] = points[i].quality;
    }

    return iec104_dispatch_send(IEC104_TI_M_ME_NC_1, IEC104_APP_MEASURE_POINT_COUNT, cot, objects, sizeof(objects), now);
}

static void iec104_handle_single_command(const iec104_frame_t *frame, uint32_t now)
{
    uint32_t ioa;
    uint8_t sco;
    uint8_t value;
    uint8_t selecting;

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_SC_NA_1, IEC104_COT_ACT, now))
    {
        return;
    }

    if ((frame->asdu[1] != 1U) ||
        (frame->asdu_length != IEC104_ASDU_HEADER_LENGTH + 4U)) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }

    ioa = iec104_asdu_ioa_read(&frame->asdu[6]);
    sco = frame->asdu[9];
    value = sco & 0x01U;
    selecting = (sco >> 7U) & 0x01U;

    if (!iec104_app_remote_control_is_known(ioa)) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
        return;
    }

    if ((sco & 0x7EU) != 0U) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }

    if (selecting != 0U) {
        if (iec104_send_command_response(frame, IEC104_COT_ACTCON, now)) {
            selected_command.ioa = ioa;
            selected_command.value = value;
            selected_command.selected = 1U;
        }
        return;
    }

    if ((selected_command.selected == 0U) ||
        (selected_command.ioa != ioa) ||
        (selected_command.value != value)) {
        selected_command.selected = 0U;
        (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM | IEC104_COT_NEGATIVE, now);
        return;
    }

    selected_command.selected = 0U;
    if ((value != 0U) &&
        iec104_send_command_response(frame, IEC104_COT_ACTTERM, now)) {
        iec104_app_remote_control_execute(ioa);
    }
    if (value == 0U) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM, now);
    }
}

static void iec104_handle_interrogation(const iec104_frame_t *frame, uint32_t now)
{
    iec104_status_point_t status_points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measure_points[IEC104_APP_MEASURE_POINT_COUNT];

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_IC_NA_1, IEC104_COT_ACT, now)) {
        return;
    }

    if ((frame->asdu[1] != 1U) ||
        (frame->asdu_length != IEC104_ASDU_HEADER_LENGTH + 4U) ||
        (iec104_asdu_ioa_read(&frame->asdu[6]) != 0U) ||
        (frame->asdu[9] != 0x14U)) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
        return;
    }

    if (!iec104_send_command_response(frame, IEC104_COT_ACTCON, now)) {
        return;
    }

    iec104_app_collect_interrogation(status_points, measure_points);
    if (!iec104_send_status_points(status_points, IEC104_COT_INTROGEN, now)) {
        return;
    }
    if (!iec104_send_measure_points(measure_points, IEC104_COT_INTROGEN, now)) {
        return;
    }
    (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM, now);
}

static void iec104_handle_tuning(const iec104_frame_t *frame, uint32_t now)
{
    iec104_tuning_object_t objects[IEC104_APP_TUNING_POINT_COUNT];
    uint8_t count;
    uint8_t offset;
    uint8_t object_index;
    uint8_t execute;

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_WS_NA_1, IEC104_COT_ACT, now)) {
        return;
    }

    count = frame->asdu[1] & 0x7FU;
    if (((frame->asdu[1] & 0x80U) != 0U) || (count == 0U) ||
        (count > IEC104_APP_TUNING_POINT_COUNT)) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }

    offset = IEC104_ASDU_HEADER_LENGTH;
    for (object_index = 0U; object_index < count; object_index++) {
        iec104_tuning_object_t *object = &objects[object_index];
        uint8_t expected_value_length;

        if ((uint16_t)offset + 4U > frame->asdu_length) {
            (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
            return;
        }

        object->ioa = iec104_asdu_ioa_read(&frame->asdu[offset]);
        object->point_index = iec104_app_tuning_index(object->ioa);
        if (object->point_index < 0) {
            (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
            return;
        }

        object->pi = frame->asdu[offset + 3U];
        if ((object->pi & 0x7CU) != 0U) {
            (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
            return;
        }

        expected_value_length = iec104_app_tuning_value_length(object->ioa);
        if ((uint16_t)offset + 4U + expected_value_length >
            frame->asdu_length) {
            (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
            return;
        }

        if (!iec104_app_tuning_value_parse(object->ioa, &frame->asdu[offset + 4U], &object->value, &object->value_length, &object->register_value)) {
            (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
            return;
        }

        offset = (uint8_t)(offset + 4U + object->value_length);
    }

    if (offset != frame->asdu_length) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }

    execute = 0U;
    for (object_index = 0U; object_index < count; object_index++) {
        const iec104_tuning_object_t *object = &objects[object_index];
        iec104_tuning_point_t *point = &tuning_points[object->point_index];

        if ((object->pi & 0x02U) != 0U) {
            point->selected = 0U;
            continue;
        }

        if ((object->pi & 0x01U) != 0U) {
            point->ioa = object->ioa;
            point->value = object->value;
            point->value_length = object->value_length;
            point->selected = 1U;
            continue;
        }

        if ((point->selected == 0U) ||
            (point->ioa != object->ioa) ||
            (point->value != object->value) ||
            (point->value_length != object->value_length) ||
            !iec104_app_tuning_write(object->ioa, object->register_value)) {
            (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM | IEC104_COT_NEGATIVE, now);
            return;
        }

        point->selected = 0U;
        execute = 1U;
    }

    (void)iec104_send_command_response(frame, execute ? IEC104_COT_ACTTERM : IEC104_COT_ACTCON, now);
}

void iec104_dispatch_init(void)
{
    iec104_status_point_t status_points[IEC104_APP_STATUS_POINT_COUNT];

    memset(&selected_command, 0, sizeof(selected_command));
    memset(tuning_points, 0, sizeof(tuning_points));
    iec104_app_collect_status_points(status_points);
    memcpy(previous_status_points, status_points, sizeof(previous_status_points));
}

void iec104_dispatch_reset(void)
{
    selected_command.selected = 0U;
    memset(tuning_points, 0, sizeof(tuning_points));
}

void iec104_dispatch_handle(const iec104_frame_t *frame, uint32_t now)
{
    if (frame->asdu_length < IEC104_ASDU_HEADER_LENGTH) {
        return;
    }

    switch (frame->asdu[0]) {
    case IEC104_TI_C_SC_NA_1:
        iec104_handle_single_command(frame, now);
        break;

    case IEC104_TI_C_IC_NA_1:
        iec104_handle_interrogation(frame, now);
        break;

    case IEC104_TI_C_WS_NA_1:
        iec104_handle_tuning(frame, now);
        break;

    default:
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_TYPE, now);
        break;
    }
}

void iec104_dispatch_send_spontaneous(uint32_t now)
{
    iec104_status_point_t points[IEC104_APP_STATUS_POINT_COUNT];
    uint8_t object[4];
    uint8_t i;

    iec104_app_collect_status_points(points);
    for (i = 0U; i < IEC104_APP_STATUS_POINT_COUNT; i++) {
        if ((points[i].value == previous_status_points[i].value) &&
            (points[i].quality == previous_status_points[i].quality)) {
            continue;
        }

        iec104_asdu_ioa_write(object, points[i].ioa);
        object[3] = points[i].value | points[i].quality;
        if (!iec104_dispatch_send(IEC104_TI_M_SP_NA_1, 1U, IEC104_COT_SPONT, object, sizeof(object), now)) {
            break;
        }
        previous_status_points[i] = points[i];
    }
}



