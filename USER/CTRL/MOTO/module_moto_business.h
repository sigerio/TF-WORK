#ifndef MODULE_MOTO_BUSINESS_H
#define MODULE_MOTO_BUSINESS_H

#ifdef __cplusplus
extern "C" {
#endif

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

void module_moto_business_update(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MOTO_BUSINESS_H */
