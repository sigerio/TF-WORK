#ifndef TF_USR_EVENT_TASK_H
#define TF_USR_EVENT_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TF_USR_EVENT_MODBUS_BASE
#define TF_USR_EVENT_MODBUS_BASE               0U
#endif

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

void tf_usr_event_task_init(void);
int8_t tf_usr_event_task_post_write(uint8_t event_id,
                                    uint16_t ref,
                                    uint16_t value);
void tf_usr_event_task_trigger(uint8_t event_id);

#ifdef __cplusplus
}
#endif

#endif /* TF_USR_EVENT_TASK_H */
