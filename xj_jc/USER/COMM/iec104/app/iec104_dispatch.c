#include "COMM/iec104/app/iec104_dispatch.h"

#include <stddef.h>
#include <string.h>

#include "COMM/iec104/core/iec104_asdu.h"
#include "COMM/iec104/core/iec104_session.h"
#include "DEBUG/debug_log.h"
#include "COMM/iec104/app/iec104_app.h"

#define IEC104_COMMON_ADDRESS 0x0001U
#define IEC104_MEASURE_POINTS_PER_ASDU 16U

/* 单点遥控采用“选择后执行”，此处保存最近一次已确认选择。 */
typedef struct {
    uint32_t ioa;      /* 已选择的遥控信息对象地址。 */
    uint8_t value;     /* 已选择的 SCO 状态值。 */
    uint8_t selected;  /* 是否存在待执行的选择。 */
} iec104_single_command_t;

#define IEC104_TUNING_OBJECT_MAX 16U

typedef struct {
    uint32_t ioa;
    uint32_t value;
    uint8_t pi;
    uint8_t value_length;
} iec104_tuning_object_t;

typedef struct {
    iec104_tuning_object_t objects[IEC104_TUNING_OBJECT_MAX];
    uint8_t count;
} iec104_tuning_selection_t;

static iec104_single_command_t selected_command;
static iec104_tuning_selection_t tuning_selections[3];
/* 遥调逐条请求/应答；暂存尚未进入发送队列的应答，不重做下游写入。 */
static uint8_t tuning_response[IEC104_MAX_ASDU_LENGTH];
static uint8_t tuning_response_length;
static iec104_status_point_t previous_status_points[IEC104_APP_STATUS_POINT_COUNT];
/* 总召保存同一次采集的快照，窗口满后只重试尚未入队的帧。 */
static iec104_status_point_t interrogation_status[IEC104_APP_STATUS_POINT_COUNT];
static iec104_measure_point_t interrogation_measure[IEC104_APP_MEASURE_POINT_COUNT];
static uint8_t interrogation_phase;
static uint8_t interrogation_offset;

static uint8_t iec104_flush_tuning_response(uint32_t now)
{
    if (tuning_response_length == 0U) return 1U;
    if (!iec104_session_send_asdu(tuning_response, tuning_response_length, now)) return 0U;
    tuning_response_length = 0U;
    return 1U;
}

static uint8_t iec104_dispatch_send(uint8_t type, uint8_t vsq, uint8_t cot, const uint8_t *objects, uint8_t objects_length, uint32_t now)
{
    uint8_t asdu[IEC104_MAX_ASDU_LENGTH];
    uint8_t asdu_length;

    asdu_length = iec104_asdu_build(type, vsq, cot, IEC104_COMMON_ADDRESS, objects, objects_length, asdu, sizeof(asdu));
    if (asdu_length == 0U) {
        return 0U;
    }

    if (!iec104_flush_tuning_response(now)) return 0U;
    return iec104_session_send_asdu(asdu, asdu_length, now);
}

static uint8_t iec104_send_command_response(const iec104_frame_t *frame, uint8_t cot, uint32_t now)
{
    if (frame->asdu[0] == IEC104_TI_C_WS_NA_1) {
        if (tuning_response_length != 0U) return 0U;
        tuning_response_length = iec104_asdu_build(frame->asdu[0], frame->asdu[1], cot, IEC104_COMMON_ADDRESS,
            &frame->asdu[IEC104_ASDU_HEADER_LENGTH], (uint8_t)(frame->asdu_length - IEC104_ASDU_HEADER_LENGTH),
            tuning_response, sizeof(tuning_response));
        if (tuning_response_length == 0U) return 0U;
        (void)iec104_flush_tuning_response(now);
        return 1U; /* 应答已入队或已可靠暂存，预置可以生效。 */
    }
    /* 命令响应原样回送 TI、VSQ 和信息对象，只替换传送原因。 */
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

void iec104_dispatch_service(uint32_t now)
{
    uint8_t objects[IEC104_MEASURE_POINTS_PER_ASDU * 8U];
    if (!iec104_session_data_transfer_started()) return;
    if (!iec104_flush_tuning_response(now)) return;
    while (interrogation_phase != 0U) {
        uint8_t count;
        uint8_t width;
        uint8_t type;
        if (interrogation_phase == 3U) {
            static const uint8_t end_object[] = { 0U, 0U, 0U, 0x14U };
            if (!iec104_dispatch_send(IEC104_TI_C_IC_NA_1, 1U, IEC104_COT_ACTTERM, end_object, sizeof(end_object), now)) return;
            interrogation_phase = 0U;
            return;
        }
        count = (interrogation_phase == 1U ? IEC104_APP_STATUS_POINT_COUNT : IEC104_APP_MEASURE_POINT_COUNT) - interrogation_offset;
        if (count > IEC104_MEASURE_POINTS_PER_ASDU) count = IEC104_MEASURE_POINTS_PER_ASDU;
        width = interrogation_phase == 1U ? 4U : 8U;
        type = interrogation_phase == 1U ? IEC104_TI_M_SP_NA_1 : IEC104_TI_M_ME_NC_1;
        for (uint8_t i = 0U; i < count; i++) {
            if (interrogation_phase == 1U) {
                const iec104_status_point_t *point = &interrogation_status[interrogation_offset + i];
                iec104_asdu_ioa_write(&objects[i * width], point->ioa);
                objects[i * width + 3U] = point->value | point->quality;
            } else {
                const iec104_measure_point_t *point = &interrogation_measure[interrogation_offset + i];
                iec104_asdu_ioa_write(&objects[i * width], point->ioa);
                memcpy(&objects[i * width + 3U], &point->value, sizeof(point->value));
                objects[i * width + 7U] = point->quality;
            }
        }
        if (!iec104_dispatch_send(type, count, IEC104_COT_INTROGEN, objects, count * width, now)) return;
        interrogation_offset += count;
        if (interrogation_offset == (interrogation_phase == 1U ? IEC104_APP_STATUS_POINT_COUNT : IEC104_APP_MEASURE_POINT_COUNT)) {
            interrogation_offset = 0U;
            interrogation_phase++;
        }
    }
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

    if ((frame->asdu[1] != 1U) || (frame->asdu_length != IEC104_ASDU_HEADER_LENGTH + 4U)) {
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

    if (!iec104_app_remote_control_allowed(ioa)) {
        selected_command.selected = 0U;
        (void)iec104_send_command_response(frame,
            (selecting ? IEC104_COT_ACTCON : IEC104_COT_ACTTERM) | IEC104_COT_NEGATIVE, now);
        return;
    }

    /* 选择报文只有在 ACTCON 成功入队后才生效。 */
    if (selecting != 0U) {
        DEBUG("APP", "IEC104 YK id=0x%06lX start value=%u select=1\n", (unsigned long)ioa, (unsigned)value);
        if (iec104_send_command_response(frame, IEC104_COT_ACTCON, now)) {
            selected_command.ioa = ioa;
            selected_command.value = value;
            selected_command.selected = 1U;
            DEBUG("APP", "IEC104 YK id=0x%06lX selected value=%u\n", (unsigned long)ioa, (unsigned)value);
        }
        return;
    }

    /* 执行报文的 IOA 和命令值必须与已确认的选择报文一致。 */
    if ((selected_command.selected == 0U) || (selected_command.ioa != ioa) || (selected_command.value != value)) {
        selected_command.selected = 0U;
        DEBUG("APP", "IEC104 YK id=0x%06lX execute rejected value=%u\n", (unsigned long)ioa, (unsigned)value);
        (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM | IEC104_COT_NEGATIVE, now);
        return;
    }

    selected_command.selected = 0U;
    DEBUG("APP", "IEC104 YK id=0x%06lX execute value=%u\n", (unsigned long)ioa, (unsigned)value);
    if ((value != 0U) && iec104_send_command_response(frame, IEC104_COT_ACTTERM, now)) {
        iec104_app_remote_control_execute(ioa);
    }
    if (value == 0U) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM, now);
    }
}

static void iec104_handle_interrogation(const iec104_frame_t *frame, uint32_t now)
{

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_IC_NA_1, IEC104_COT_ACT, now)) {
        return;
    }

    if ((frame->asdu[1] != 1U) || (frame->asdu_length != IEC104_ASDU_HEADER_LENGTH + 4U) ||
        (iec104_asdu_ioa_read(&frame->asdu[6]) != 0U) || (frame->asdu[9] != 0x14U)) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
        return;
    }

    if (interrogation_phase != 0U) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }
    if (!iec104_send_command_response(frame, IEC104_COT_ACTCON, now)) return;
    iec104_app_collect_interrogation(interrogation_status, interrogation_measure);
    interrogation_offset = 0U;
    interrogation_phase = 1U;
    iec104_dispatch_service(now);
}

static void iec104_handle_tuning(const iec104_frame_t *frame, uint32_t now)
{
    iec104_tuning_object_t objects[IEC104_TUNING_OBJECT_MAX];
    uint32_t ioas[IEC104_TUNING_OBJECT_MAX];
    uint32_t values[IEC104_TUNING_OBJECT_MAX];
    uint16_t offset = IEC104_ASDU_HEADER_LENGTH;
    uint8_t count;
    uint8_t target = 0xFFU;
    iec104_tuning_selection_t *selection;

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_WS_NA_1, IEC104_COT_ACT, now)) return;
    count = frame->asdu[1] & 0x7FU;
    if ((frame->asdu[1] & 0x80U) || count == 0U || count > IEC104_TUNING_OBJECT_MAX) goto invalid;
    /* 全帧先校验；排序、同设备、同 PI 和完整预置匹配均通过后才允许写入。 */
    for (uint8_t i = 0U; i < count; i++) {
        iec104_tuning_object_t *object = &objects[i];
        uint32_t register_value;
        uint8_t length;
        if (offset + 4U > frame->asdu_length) goto invalid;
        object->ioa = iec104_asdu_ioa_read(&frame->asdu[offset]);
        if (iec104_app_tuning_index(object->ioa) < 0) {
            (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
            return;
        }
        object->pi = frame->asdu[offset + 3U];
        if (object->pi > 2U) goto invalid;
        if (i == 0U) target = iec104_app_tuning_target(object->ioa);
        if (target != iec104_app_tuning_target(object->ioa) ||
            (target == IEC104_TUNING_TARGET_AC && count > 13U) ||
            (target == IEC104_TUNING_TARGET_WEATHER && count > 2U)) goto invalid;
        if (i != 0U && (object->ioa <= objects[i - 1U].ioa || object->pi != objects[0].pi)) goto invalid;
        length = iec104_app_tuning_value_length(object->ioa);
        if (offset + 4U + length > frame->asdu_length) goto invalid;
        if (!iec104_app_tuning_value_parse(object->ioa, &frame->asdu[offset + 4U],
            &object->value, &object->value_length, &register_value)) goto invalid;
        ioas[i] = object->ioa;
        values[i] = register_value;
        offset += 4U + length;
    }
    if (offset != frame->asdu_length) goto invalid;
    selection = &tuning_selections[target];
    if (objects[0].pi == 1U) {
        if (iec104_send_command_response(frame, IEC104_COT_ACTCON, now)) {
            memcpy(selection->objects, objects, count * sizeof(objects[0]));
            selection->count = count;
        }
        return;
    }
    if (objects[0].pi == 2U) {
        selection->count = 0U;
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON, now);
        return;
    }
    if (selection->count != count) goto mismatch;
    for (uint8_t i = 0U; i < count; i++) {
        if (selection->objects[i].ioa != objects[i].ioa || selection->objects[i].value != objects[i].value ||
            selection->objects[i].value_length != objects[i].value_length) goto mismatch;
    }
    selection->count = 0U;
    if (!iec104_app_tuning_write_group(ioas, values, count)) goto mismatch;
    (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM, now);
    return;

mismatch:
    selection->count = 0U;
    (void)iec104_send_command_response(frame, IEC104_COT_ACTTERM | IEC104_COT_NEGATIVE, now);
    return;
invalid:
    (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
}

void iec104_dispatch_init(void)
{
    iec104_status_point_t status_points[IEC104_APP_STATUS_POINT_COUNT];
    uint8_t i;

    memset(&selected_command, 0, sizeof(selected_command));
    memset(tuning_selections, 0, sizeof(tuning_selections));
    tuning_response_length = 0U;
    interrogation_phase = 0U;
    interrogation_offset = 0U;
    /* 以初始化时的实际状态作为基线，避免启动后把全部遥信误判为变位。 */
    iec104_app_collect_status_points(status_points);
    memcpy(previous_status_points, status_points, sizeof(previous_status_points));
    /* 上电已经存在的限位故障也必须在 STARTDT 后主动上送。 */
    for (i = 0U; i < IEC104_APP_STATUS_POINT_COUNT; i++) {
        if ((previous_status_points[i].ioa == IEC104_IOA_YX_DOOR_LIMIT_FAULT) ||
            (previous_status_points[i].ioa == IEC104_IOA_YX_PLATFORM_LIMIT_FAULT)) {
            previous_status_points[i].value = 0U;
        }
    }
}

void iec104_dispatch_reset(void)
{
    selected_command.selected = 0U;
    memset(tuning_selections, 0, sizeof(tuning_selections));
    tuning_response_length = 0U;
    interrogation_phase = 0U;
    interrogation_offset = 0U;
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
        /* 前一应答待发时不接受下一请求，也不覆盖或重复执行前一事务。 */
        if (tuning_response_length != 0U) {
            (void)iec104_flush_tuning_response(now);
            return;
        }
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

    if (interrogation_phase != 0U) return;
    iec104_app_collect_status_points(points);
    for (i = 0U; i < IEC104_APP_STATUS_POINT_COUNT; i++) {
        if ((points[i].value == previous_status_points[i].value) && (points[i].quality == previous_status_points[i].quality)) {
            continue;
        }

        iec104_asdu_ioa_write(object, points[i].ioa);
        object[3] = points[i].value | points[i].quality;
        if (!iec104_dispatch_send(IEC104_TI_M_SP_NA_1, 1U, IEC104_COT_SPONT, object, sizeof(object), now)) {
            break;
        }
        /* 仅发送成功后提交基线，入队失败的变位会在下个轮询周期重试。 */
        previous_status_points[i] = points[i];
    }
}
