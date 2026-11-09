#ifndef APP_STATUS_H
#define APP_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/* 1 表示对应设备当前运行或通信在线。 */
typedef enum {
    OS_DOOR_S = 0,
    OS_HELIPAD_S,
    OS_485_TH_ONLINE,
    OS_485_WIND_ONLINE,
    OS_485_WATER_ONLINE,
    OS_485_RAIN_ONLINE,
    OS_485_AC_ONLINE,
    OS_IEC104_ONLINE,
    OS_RUN_SIGNAL_COUNT,
}OS_RUN_SIGNAL_EN;

typedef union {
    struct {
        uint16_t door : 1;
        uint16_t helipad : 1;
        uint16_t th_online : 1;
        uint16_t wind_online : 1;
        uint16_t water_online : 1;
        uint16_t rain_online : 1;
        uint16_t ac_online : 1;
        uint16_t iec104_online : 1;
        uint16_t reserved : 8;
    }bits;
    uint16_t byte;
}os_state_u;

/* 设置、清除或清空运行状态位；无效状态位忽略。 */
void set_signal(OS_RUN_SIGNAL_EN signal);
void cancel_signal(OS_RUN_SIGNAL_EN signal);
void clean_signal(void);

/* 查询单项状态或取得所有状态位的快照。 */
bool is_signal_set(OS_RUN_SIGNAL_EN signal);
os_state_u get_signal_state(void);

#endif /* APP_STATUS_H */
