#ifndef MOTOR_BUSINESS_H
#define MOTOR_BUSINESS_H

#ifdef __cplusplus
extern "C" {
#endif

/* 业务命令入口：处理急停门控后，将门/平台动作交给运动同步层执行。 */
void module_moto_business_request_door_open(void);
void module_moto_business_request_door_close(void);
void module_moto_business_request_platform_extend(void);
void module_moto_business_request_platform_retract(void);

/*
 * 短按切换接口：内部依据当前相位/运行终态判定下一步应开或关、伸或缩，
 * 再复用上述单向请求。切换状态由 business 层内部维护，不暴露给调用方。
 */
void module_moto_business_toggle_door(void);
void module_moto_business_toggle_platform(void);

/* 急停控制（仅为软件门控，不切入硬件急停回路） */
void module_moto_business_estop_activate(void);
void module_moto_business_estop_release(void);

/* 周期更新业务侧的按键切换终态；应与运动同步任务同周期调用。 */
void module_moto_business_update(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_BUSINESS_H */
