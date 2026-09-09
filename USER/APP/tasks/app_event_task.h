#ifndef APP_EVENT_TASK_H
#define APP_EVENT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define USR_EVENT_TASK_OK                   0
#define USR_EVENT_TASK_ERR_EVENT            (-1)
#define USR_EVENT_TASK_ERR_ADDR             (-2)


/**
 * @brief 注册用户软事件任务。
 *
 * 必须在 os_init() 之后、投递事件之前调用。
 */
void usr_event_task_init(void);


#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_TASK_H */
