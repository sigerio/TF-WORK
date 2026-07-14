#include "module_moto_business.h"

#include <stdbool.h>
#include <stdint.h>

#include "module_moto_sync.h"

/*
 * 门与停机坪共用一套单轴状态机，差异仅由 axis_ctx_t 数据描述。
 * 抽象方向：
 *   FWD（无约束）——门:开 / 坪:收，随时可发起。
 *   CON（受约束）——门:关 / 坪:伸，发起前须另一轴到达其 FWD 终态。
 * 唯一禁止组合“门关闭 + 停机坪伸出”即由 CON 的前置条件排除。
 */

#define REVERSE_WAIT_MS 100U

/* 单轴相位（FWD/CON 含义见文件头） */
typedef enum
{
    AXIS_OVER = 0,
    AXIS_FWD_ING,       /* 无约束方向运行中 */
    AXIS_WAIT_PRECOND,  /* 等待受约束方向前置条件 */
    AXIS_CON_ING,       /* 受约束方向运行中 */
    AXIS_WAIT_REV_FWD,  /* 反向切向无约束的防抖等待 */
    AXIS_WAIT_REV_CON   /* 反向切向受约束的防抖等待 */
} axis_phase_e;

typedef struct axis_ctx
{
    axis_phase_e phase;
    uint16_t reverse_wait_ms;
    void (*sync_fwd)(void);                             /* 启动无约束方向 */
    void (*sync_con)(void);                             /* 启动受约束方向 */
    void (*sync_stop)(void);                            /* 停机 */
    bool (*at_fwd)(const module_moto_run_status_t *);   /* 处于无约束到位终态 */
    bool (*at_con)(const module_moto_run_status_t *);   /* 处于受约束到位终态 */
    struct axis_ctx *other;                             /* 另一轴（前置依赖对象） */
    bool idle_fault_con;                                /* 空闲兜底方向：true=受约束 */
} axis_ctx_t;

/* ============================ 轴到位判据 ============================ */

static bool door_at_fwd(const module_moto_run_status_t *s)
{
    return s->door == MODULE_MOTO_DOOR_STATE_OPENED;
}

static bool door_at_con(const module_moto_run_status_t *s)
{
    return s->door == MODULE_MOTO_DOOR_STATE_CLOSED;
}

static bool heliped_at_fwd(const module_moto_run_status_t *s)
{
    return s->heliped == MODULE_MOTO_HELIPED_STATE_RETRACTED;
}

static bool heliped_at_con(const module_moto_run_status_t *s)
{
    return s->heliped == MODULE_MOTO_HELIPED_STATE_EXTENDED;
}

/* ============================ 两轴实例 ============================ */

static axis_ctx_t door_axis;
static axis_ctx_t heliped_axis;

static axis_ctx_t door_axis = {
    .phase = AXIS_OVER,
    .reverse_wait_ms = 0U,
    .sync_fwd = module_moto_sync_request_door_open,
    .sync_con = module_moto_sync_request_door_close,
    .sync_stop = module_moto_sync_stop_door,
    .at_fwd = door_at_fwd,
    .at_con = door_at_con,
    .other = &heliped_axis,
    .idle_fault_con = false,
};

static axis_ctx_t heliped_axis = {
    .phase = AXIS_OVER,
    .reverse_wait_ms = 0U,
    .sync_fwd = module_moto_sync_request_heliped_retract,
    .sync_con = module_moto_sync_request_heliped_extend,
    .sync_stop = module_moto_sync_stop_heliped,
    .at_fwd = heliped_at_fwd,
    .at_con = heliped_at_con,
    .other = &door_axis,
    .idle_fault_con = true,
};

/* ============================ 通用引擎 ============================ */

/* 反向防抖计时推进一周期，到点清零并返回 true */
static bool reverse_wait_elapsed(axis_ctx_t *a)
{
    a->reverse_wait_ms += MODULE_MOTO_SYNC_MONITOR_PERIOD_MS;

    if (a->reverse_wait_ms < REVERSE_WAIT_MS)
    {
        return false;
    }

    a->reverse_wait_ms = 0U;
    return true;
}

/* 受约束方向前置条件：另一轴已到 FWD 终态且处于收/空闲相位 */
static bool con_precond_ready(const axis_ctx_t *a, const module_moto_run_status_t *s)
{
    return a->other->at_fwd(s) &&
           ((a->other->phase == AXIS_FWD_ING) || (a->other->phase == AXIS_OVER));
}

/* 请求无约束方向（无前置，含反向防抖与幂等折叠） */
static void axis_request_fwd(axis_ctx_t *a)
{
    if ((a->phase == AXIS_FWD_ING) || (a->phase == AXIS_WAIT_REV_FWD))
    {
        return;
    }

    if (a->phase == AXIS_CON_ING)
    {
        a->sync_stop();
        a->reverse_wait_ms = 0U;
        a->phase = AXIS_WAIT_REV_FWD;
        return;
    }

    if (a->phase == AXIS_WAIT_REV_CON)
    {
        a->phase = AXIS_WAIT_REV_FWD;
        return;
    }

    a->sync_fwd();
    a->phase = AXIS_FWD_ING;
}

/* 请求受约束方向：先强制另一轴到 FWD（last-wins），再按当前相位处理 */
static void axis_request_con(axis_ctx_t *a)
{
    if ((a->phase == AXIS_WAIT_PRECOND) ||
        (a->phase == AXIS_CON_ING) ||
        (a->phase == AXIS_WAIT_REV_CON))
    {
        return;
    }

    axis_request_fwd(a->other);

    if (a->phase == AXIS_FWD_ING)
    {
        a->sync_stop();
        a->reverse_wait_ms = 0U;
        a->phase = AXIS_WAIT_REV_CON;
        return;
    }

    if (a->phase == AXIS_WAIT_REV_FWD)
    {
        a->phase = AXIS_WAIT_REV_CON;
        return;
    }

    a->phase = AXIS_WAIT_PRECOND;
}

/* 短按切换：依当前相位/终态判定反向 */
static void axis_toggle(axis_ctx_t *a)
{
    module_moto_run_status_t status;

    switch (a->phase)
    {
    case AXIS_FWD_ING:
    case AXIS_WAIT_REV_FWD:
        axis_request_con(a);
        break;

    case AXIS_WAIT_PRECOND:
    case AXIS_CON_ING:
    case AXIS_WAIT_REV_CON:
        axis_request_fwd(a);
        break;

    case AXIS_OVER:
    default:
        module_moto_sync_get_run_status(&status);
        if (a->at_fwd(&status))
        {
            axis_request_con(a);
        }
        else if (a->at_con(&status))
        {
            axis_request_fwd(a);
        }
        else if (a->idle_fault_con)
        {
            axis_request_con(a);
        }
        else
        {
            axis_request_fwd(a);
        }
        break;
    }
}

/* 单轴相位推进；到位判据取自 sync 层 run_status 终态 */
static void axis_update(axis_ctx_t *a, const module_moto_run_status_t *s)
{
    switch (a->phase)
    {
    case AXIS_FWD_ING:
        if (a->at_fwd(s))
        {
            a->phase = AXIS_OVER;
        }
        break;

    case AXIS_WAIT_PRECOND:
        if (con_precond_ready(a, s))
        {
            a->sync_con();
            a->phase = AXIS_CON_ING;
        }
        break;

    case AXIS_CON_ING:
        if (a->at_con(s))
        {
            a->phase = AXIS_OVER;
        }
        break;

    case AXIS_WAIT_REV_FWD:
        if (reverse_wait_elapsed(a))
        {
            a->sync_fwd();
            a->phase = AXIS_FWD_ING;
        }
        break;

    case AXIS_WAIT_REV_CON:
        if (reverse_wait_elapsed(a))
        {
            /* 防抖到点复查前置：满足才启动受约束方向，否则挂起等待 */
            if (con_precond_ready(a, s))
            {
                a->sync_con();
                a->phase = AXIS_CON_ING;
            }
            else
            {
                a->phase = AXIS_WAIT_PRECOND;
            }
        }
        break;

    case AXIS_OVER:
    default:
        break;
    }
}

/* ============================ 对外接口 ============================ */

void module_moto_business_request_door_open(void)
{
    axis_request_fwd(&door_axis);
}

void module_moto_business_request_door_close(void)
{
    axis_request_con(&door_axis);
}

void module_moto_business_request_heliped_extend(void)
{
    axis_request_con(&heliped_axis);
}

void module_moto_business_request_heliped_retract(void)
{
    axis_request_fwd(&heliped_axis);
}

void module_moto_business_toggle_door(void)
{
    axis_toggle(&door_axis);
}

void module_moto_business_toggle_heliped(void)
{
    axis_toggle(&heliped_axis);
}

void module_moto_business_update(void)
{
    module_moto_run_status_t run_status;

    module_moto_sync_get_run_status(&run_status);

    /* 门轴先更新：其相位变化会被同周期停机坪轴的前置判断读取 */
    axis_update(&door_axis, &run_status);
    axis_update(&heliped_axis, &run_status);
}
