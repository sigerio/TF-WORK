#ifndef APP_EVENTS_H
#define APP_EVENTS_H

/* 用户软事件编号：前三项为 RS-485 写请求，其余项为本机按键和运动状态事件。 */
typedef enum {
    USR_EVENT_485_XM8537_WRITE = 0,
    USR_EVENT_485_AC_STATUS_WRITE,
    USR_EVENT_485_AC_PARAM_WRITE,
    USR_EVENT_EM_STOP,
    USR_EVENT_EM_STOP_RECOVER,
    USR_EVENT_KEY_TRIG,
    USR_EVENT_DOOR_STATE,
    USR_EVENT_PLATFORM_STATE,
    USR_EVENT_ALL_COUNT, /* 事件编号总数，不可用于投递。 */
} usr_event_id_t;

#endif /* APP_EVENTS_H */
