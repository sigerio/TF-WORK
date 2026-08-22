#ifndef KEY_EVENT_H
#define KEY_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* 消费已扫描到的按键事件，并转换为门/平台业务短按命令。 */
void tf_key_event_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* KEY_EVENT_H */
