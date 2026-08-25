#ifndef MOTOR_MOTION_H
#define MOTOR_MOTION_H

#include "motor_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 同步调度周期（ms）。
 * 需与注册到 soft timer 的调用周期一致。
 */
#ifndef MODULE_MOTO_SYNC_MONITOR_PERIOD_MS
#define MODULE_MOTO_SYNC_MONITOR_PERIOD_MS 10U
#endif

/**
 * 门业务运行状态（相对机库流程，不直接映射电机 DIR/POS）。
 * - CLOSED       : 门已关
 * - OPENED       : 门已开（全开到位）
 * - SAFE_OPEN    : 预留安全开门状态，当前未使用
 * - FAULT        : 门故障
 * - INTERMEDIATE : 未处于完整开门或关门终态
 */
typedef enum
{
    MODULE_MOTO_DOOR_STATE_FAULT = 0,
    MODULE_MOTO_DOOR_STATE_CLOSED,
    MODULE_MOTO_DOOR_STATE_OPENED,
    MODULE_MOTO_DOOR_STATE_INTERMEDIATE,
} module_moto_door_state_e;

/**
 * 平台（停机坪）业务运行状态。
 * - EXTENDED     : 完全伸出
 * - RETRACTED    : 完全归位
 * - SAFE_RETRACT : 预留安全回收状态，当前未使用
 * - INTERMEDIATE : 未处于完全伸出或完全归位终态
 */
typedef enum
{
    MODULE_MOTO_PLATFORM_STATE_FAULT = 0,
    MODULE_MOTO_PLATFORM_STATE_EXTENDED,
    MODULE_MOTO_PLATFORM_STATE_RETRACTED,
    MODULE_MOTO_PLATFORM_STATE_INTERMEDIATE,
} module_moto_platform_state_e;

/**
 * 门 / 平台业务运行状态及硬件信号快照。
 * @note 供后续流程编排与调试器 Watch 读取；由 sync 模块内部维护。
 */
typedef struct
{
    module_moto_door_state_e door;              /* 门业务终态或中间/故障状态。 */
    module_moto_platform_state_e platform;      /* 平台业务终态或中间/故障状态。 */
    module_moto_driver_status_t platform_motor; /* 平台电机驱动及反馈快照。 */
    module_moto_driver_status_t door1_motor;    /* 左门电机驱动及反馈快照。 */
    module_moto_driver_status_t door2_motor;    /* 右门电机驱动及反馈快照。 */
    uint8_t platform_extend_limit_active;       /* 平台伸出限位是否有效。 */
    uint8_t platform_retract_limit_active;      /* 平台收回限位是否有效。 */
    uint8_t door_open_limit_active;             /* 两门打开到位限位是否有效。 */
    uint8_t door_close_limit_active;            /* 两门关闭到位限位是否有效。 */
} module_moto_run_status_t;

/** 在电机模块初始化完成后，根据当前限位建立业务运行状态。 */
void module_moto_sync_init(void);

/**
 * @brief  读取门 / 平台业务运行状态。
 * @param  status  输出缓存，不可为 NULL
 */
void module_moto_sync_get_run_status(module_moto_run_status_t *status);

/* 立即停止对应设备的所有电机，并终止当前运动请求。 */
void module_moto_sync_stop_door(void);
void module_moto_sync_stop_platform(void);

/**
 * @brief  请求双门打开。
 * @note   BACKWARD 方向 / 目标位置 MODULE_MOTO_POS_BACKWARD（开到位限位）。
 */
void module_moto_sync_request_door_open(void);

/**
 * @brief  请求双门关闭。
 * @note   FORWARD 方向 / 目标位置 MODULE_MOTO_POS_FORWARD（关到位限位）。
 */
void module_moto_sync_request_door_close(void);

/**
 * @brief  请求平台（停机坪）伸出。
 * @note   单电机动作，FORWARD 方向 / 目标位置 MODULE_MOTO_POS_FORWARD。
 */
void module_moto_sync_request_platform_extend(void);

/**
 * @brief  请求平台（停机坪）收回。
 * @note   单电机动作，BACKWARD 方向 / 目标位置 MODULE_MOTO_POS_BACKWARD。
 */
void module_moto_sync_request_platform_retract(void);

/**
 * @brief  运动同步周期任务。
 * @note   应按 MODULE_MOTO_SYNC_MONITOR_PERIOD_MS 周期调用。
 *         推进门 / 平台状态机：启动、到位停机。
 */
void module_moto_sync_update(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_MOTION_H */
