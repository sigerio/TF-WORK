#ifndef TF_USR_EVENT_TASK_H
#define TF_USR_EVENT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TF_USR_EVENT_MODBUS_BASE
#define TF_USR_EVENT_MODBUS_BASE               0U
#endif

#define TF_USR_EVENT_TASK_OK                   0
#define TF_USR_EVENT_TASK_ERR_EVENT            (-1)
#define TF_USR_EVENT_TASK_ERR_ADDR             (-2)

typedef enum {
    TF_USR_EVENT_MODBUS_XM7828_WRITE =
        TF_USR_EVENT_MODBUS_BASE,
    TF_USR_EVENT_MODBUS_XM8189_WRITE,
    TF_USR_EVENT_MODBUS_XM8537_WRITE,
    TF_USR_EVENT_MODBUS_XM9505_WRITE,
    TF_USR_EVENT_MODBUS_AC_STATUS_WRITE,
    TF_USR_EVENT_MODBUS_AC_PARAM_WRITE,
    TF_USR_EVENT_MODBUS_COUNT,
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
