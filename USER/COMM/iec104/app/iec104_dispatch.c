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

/* 每个遥调点独立保存选择值，执行报文必须与该值完全一致。 */
typedef struct {
    uint32_t ioa;         /* 该遥调点的信息对象地址。 */
    uint32_t value;       /* 已选择的参数原始值。 */
    uint8_t value_length; /* 参数值在线序中的字节数。 */
    uint8_t selected;     /* 是否存在待固化的选择。 */
} iec104_tuning_point_t;

typedef struct {
    int8_t point_index;
    uint32_t ioa;         /* 当前解析对象的信息对象地址。 */
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

static uint8_t iec104_send_status_points(const iec104_status_point_t *points, uint8_t cot, uint32_t now)
{
    uint8_t objects[IEC104_APP_STATUS_POINT_COUNT * 4U];
    uint8_t i;

    /* SQ=0：每个单点信息对象均编码 3 字节 IOA 和 1 字节 SIQ。 */
    for (i = 0U; i < IEC104_APP_STATUS_POINT_COUNT; i++) {
        iec104_asdu_ioa_write(&objects[i * 4U], points[i].ioa);
        objects[i * 4U + 3U] = points[i].value | points[i].quality;
    }

    return iec104_dispatch_send(IEC104_TI_M_SP_NA_1, IEC104_APP_STATUS_POINT_COUNT, cot, objects, sizeof(objects), now);
}

static uint8_t iec104_send_measure_points(const iec104_measure_point_t *points, uint8_t cot, uint32_t now)
{
    uint8_t objects[IEC104_MEASURE_POINTS_PER_ASDU * 8U];
    uint8_t start = 0U;
    uint8_t count;
    uint8_t i;

    /* 固定 16 点一帧；32 个短浮点遥测对象在总召时拆为两帧。 */
    while (start < IEC104_APP_MEASURE_POINT_COUNT) {
        count = (uint8_t)(IEC104_APP_MEASURE_POINT_COUNT - start);
        if (count > IEC104_MEASURE_POINTS_PER_ASDU) {
            count = IEC104_MEASURE_POINTS_PER_ASDU;
        }
        for (i = 0U; i < count; i++) {
            const iec104_measure_point_t *point = &points[start + i];

            iec104_asdu_ioa_write(&objects[i * 8U], point->ioa);
            memcpy(&objects[i * 8U + 3U], &point->value, sizeof(point->value));
            objects[i * 8U + 7U] = point->quality;
        }
        if (!iec104_dispatch_send(IEC104_TI_M_ME_NC_1, count, cot, objects, (uint8_t)(count * 8U), now)) {
            return 0U;
        }
        start = (uint8_t)(start + count);
    }

    return 1U;
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
    iec104_status_point_t status_points[IEC104_APP_STATUS_POINT_COUNT];
    iec104_measure_point_t measure_points[IEC104_APP_MEASURE_POINT_COUNT];

    if (!iec104_asdu_header_is_valid(frame, IEC104_TI_C_IC_NA_1, IEC104_COT_ACT, now)) {
        return;
    }

    if ((frame->asdu[1] != 1U) || (frame->asdu_length != IEC104_ASDU_HEADER_LENGTH + 4U) ||
        (iec104_asdu_ioa_read(&frame->asdu[6]) != 0U) || (frame->asdu[9] != 0x14U)) {
        (void)iec104_send_command_response(frame, IEC104_COT_UNKNOWN_IOA, now);
        return;
    }

    /* 总召唤顺序固定为激活确认、遥信、遥测、激活终止。 */
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
    if (((frame->asdu[1] & 0x80U) != 0U) || (count == 0U) || (count > IEC104_APP_TUNING_POINT_COUNT)) {
        (void)iec104_send_command_response(frame, IEC104_COT_ACTCON | IEC104_COT_NEGATIVE, now);
        return;
    }

    /*
     * 第一阶段只解析并校验全部对象，不修改选择状态或设备参数；
     * 任一对象非法时整帧拒绝，避免半帧已经产生业务副作用。
     */
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
        if ((uint16_t)offset + 4U + expected_value_length > frame->asdu_length) {
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

    /* PI bit1 取消选择，bit0 建立选择，两位均为 0 时执行已选择值。 */
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

        if ((point->selected == 0U) || (point->ioa != object->ioa) || (point->value != object->value) || (point->value_length != object->value_length) || !iec104_app_tuning_write(object->ioa,
            object->register_value)) {
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
    uint8_t i;

    memset(&selected_command, 0, sizeof(selected_command));
    memset(tuning_points, 0, sizeof(tuning_points));
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
