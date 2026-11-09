#ifndef OS_INTERNAL_H
#define OS_INTERNAL_H

#include <stdint.h>

/* 仅供调度器及其子模块使用；应用通过 os.h 中的接口注册任务。 */
void os_timer_init(void);
void os_timer_process(uint32_t now);
void os_event_init(void);
void os_event_process(void);
void os_pool_init(void);
int8_t os_pool_process(void);

#endif /* OS_INTERNAL_H */
