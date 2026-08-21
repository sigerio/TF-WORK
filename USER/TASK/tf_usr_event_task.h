#ifndef TF_USR_EVENT_TASK_H
#define TF_USR_EVENT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif



#define TF_USR_EVENT_TASK_OK                   0
#define TF_USR_EVENT_TASK_ERR_EVENT            (-1)
#define TF_USR_EVENT_TASK_ERR_ADDR             (-2)

typedef enum {
    TF_USR_EVENT_485_XM7828_WRITE = 0,
    TF_USR_EVENT_485_XM8189_WRITE,
    TF_USR_EVENT_485_XM8537_WRITE,
    TF_USR_EVENT_485_XM9505_WRITE,
    TF_USR_EVENT_485_AC_STATUS_WRITE,
    TF_USR_EVENT_485_AC_PARAM_WRITE,
    TF_USR_EVENT_EM_STOP,
    TF_USR_EVENT_EM_STOP_RECOVER,
    TF_USR_EVENT_KEY_TRIG,
    TF_USR_EVENT_DOOR_STATE,
    TF_USR_EVENT_PLATFORM_STATE,
    TF_USR_EVENT_ALL_COUNT,
} tf_usr_event_id_t;

/**
 * @brief 注册用户软事件任务。
 *
 * 必须在 tf_os_init() 之后、投递事件之前调用。
 */
void tf_usr_event_task_init(void);


#ifdef __cplusplus
}
#endif

#endif /* TF_USR_EVENT_TASK_H */
