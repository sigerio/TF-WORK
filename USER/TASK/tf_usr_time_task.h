#ifndef TF_USR_TIME_TASK_H
#define TF_USR_TIME_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* 在 OS 初始化后注册本工程的周期任务；同一任务只应注册一次。 */
void tf_usr_time_task_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TF_USR_TIME_TASK_H */
