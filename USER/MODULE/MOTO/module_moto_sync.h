#ifndef MODULE_MOTO_SYNC_H
#define MODULE_MOTO_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 同步调度周期（ms）。
 * 需与注册到 soft timer 的调用周期一致。
 * 开启门同步宏后，堵转计时依赖该值累加。
 */
#ifndef MODULE_MOTO_SYNC_MONITOR_PERIOD_MS
#define MODULE_MOTO_SYNC_MONITOR_PERIOD_MS 10U
#endif

#ifndef MODULE_MOTO_DOOR_SYNC_ENABLE
#define MODULE_MOTO_DOOR_SYNC_ENABLE 0U
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
    MODULE_MOTO_DOOR_STATE_SAFE_OPEN,
    MODULE_MOTO_DOOR_STATE_OPENED,
    MODULE_MOTO_DOOR_STATE_INTERMEDIATE,
} module_moto_door_state_e;

/**
 * 停机坪（heliped）业务运行状态。
 * - EXTENDED     : 完全伸出
 * - RETRACTED    : 完全归位
 * - SAFE_RETRACT : 预留安全回收状态，当前未使用
 * - INTERMEDIATE : 未处于完全伸出或完全归位终态
 */
typedef enum
{
    MODULE_MOTO_HELIPED_STATE_FAULT = 0,
    MODULE_MOTO_HELIPED_STATE_EXTENDED,
    MODULE_MOTO_HELIPED_STATE_SAFE_RETRACT,
    MODULE_MOTO_HELIPED_STATE_RETRACTED,
    MODULE_MOTO_HELIPED_STATE_INTERMEDIATE,
} module_moto_heliped_state_e;

/**
 * 门 / 停机坪业务运行状态快照。
 * @note 仅描述业务语义，不包含电机 phase/direction 等运动层细节。
 *       供后续流程编排读取；由 sync 模块内部维护。
 */
typedef struct
{
    module_moto_door_state_e door;
    module_moto_heliped_state_e heliped;
} module_moto_run_status_t;

/** 在电机模块初始化完成后，根据当前限位建立业务运行状态。 */
void module_moto_sync_init(void);

/**
 * @brief  读取门 / 停机坪业务运行状态。
 * @param  status  输出缓存，不可为 NULL
 */
void module_moto_sync_get_run_status(module_moto_run_status_t *status);

void module_moto_sync_stop_door(void);
void module_moto_sync_stop_heliped(void);

/**
 * @brief  请求双门打开。
 * @note   仅设置目标与阶段，实际启停由 module_moto_sync_update() 执行。
 *         FORWARD 方向 / 目标位置 MODULE_MOTO_POS_FORWARD。
 */
void module_moto_sync_request_door_open(void);

/**
 * @brief  请求双门关闭。
 * @note   仅设置目标与阶段，实际启停由 module_moto_sync_update() 执行。
 *         BACKWARD 方向 / 目标位置 MODULE_MOTO_POS_BACKWARD。
 */
void module_moto_sync_request_door_close(void);

/**
 * @brief  请求停机坪（heliped）伸出。
 * @note   单电机动作，无双门同步逻辑。
 *         FORWARD 方向 / 目标位置 MODULE_MOTO_POS_FORWARD。
 */
void module_moto_sync_request_heliped_extend(void);

/**
 * @brief  请求停机坪（heliped）收回。
 * @note   单电机动作，无双门同步逻辑。
 *         BACKWARD 方向 / 目标位置 MODULE_MOTO_POS_BACKWARD。
 */
void module_moto_sync_request_heliped_retract(void);

/**
 * @brief  运动同步周期任务。
 * @note   应按 MODULE_MOTO_SYNC_MONITOR_PERIOD_MS 周期调用。
 *         推进门 / 停机坪状态机：启动、到位停机。
 *         开启门同步宏后，门运行期额外执行霍尔同步和堵转检测。
 */
void module_moto_sync_update(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MOTO_SYNC_H */
