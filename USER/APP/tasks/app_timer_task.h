#ifndef APP_TIMER_TASK_H
#define APP_TIMER_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* 按键、电机和 OS 初始化后调用：采样急停并注册周期任务，只调用一次。 */
void usr_time_task_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TIMER_TASK_H */
