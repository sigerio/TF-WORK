#ifndef APP_FAULT_H
#define APP_FAULT_H

#include <stdint.h>

#define APP_FAULT_PERIOD_MS 10U

/* 数字越小优先级越高；NONE 表示当前没有收集到故障。等级仅用于归类，不新增保护动作。 */
typedef enum {
    APP_FAULT_PRIORITY_NONE = 0,
    APP_FAULT_PRIORITY_P1 = 1, /* 紧急安全：急停、限位冲突、有效水浸。 */
    APP_FAULT_PRIORITY_P2 = 2, /* 运行告警：启动接口失败、空调运行/环境告警。 */
    APP_FAULT_PRIORITY_P3 = 3  /* 监测失效：设备离线或传感器数据故障。 */
} app_fault_priority_e;

/* 1 表示本次采集存在该故障；只保存当前状态，不锁存历史故障。
   bits 和 levels 按相同位序分组；bit 0..24 同时是 B4 板间协议位定义，禁止重排，保留位为 0。 */
typedef union {
    struct {
        /* P1，bit 0..3。急停作为安全输入收集，不代表已诊断具体损坏原因。 */
        uint32_t emergency_stop : 1;
        uint32_t door_limit_conflict : 1;
        uint32_t platform_limit_conflict : 1;
        uint32_t water_detected : 1;
        /* P2，bit 4..11。启动失败只表示软件驱动接口失败，不代表堵转。 */
        uint32_t door_start_failed : 1;
        uint32_t platform_start_failed : 1;
        uint32_t ac_high_temp : 1;
        uint32_t ac_low_temp : 1;
        uint32_t ac_humidity_high : 1;
        uint32_t ac_condenser_high_pressure : 1;
        uint32_t ac_evaporator_freeze : 1;
        uint32_t ac_cooling_alarm : 1;
        /* P3，bit 12..24。上电尚未取得响应也按设备离线收集。 */
        uint32_t th_offline : 1;
        uint32_t wind_offline : 1;
        uint32_t water_offline : 1;
        uint32_t rain_offline : 1;
        uint32_t ac_offline : 1;
        uint32_t th_sensor_fault : 1;
        uint32_t wind_sensor_fault : 1;
        uint32_t water_sensor_fault : 1;
        uint32_t rain_sensor_fault : 1;
        uint32_t ac_cabinet_sensor_fault : 1;
        uint32_t ac_evaporator_sensor_fault : 1;
        uint32_t ac_condenser_sensor_fault : 1;
        uint32_t ac_humidity_sensor_fault : 1;
        uint32_t reserved : 7;
    } bits;
    struct {
        uint32_t p1 : 4;
        uint32_t p2 : 8;
        uint32_t p3 : 13;
        uint32_t reserved : 7;
    } levels;
    uint32_t raw;
} app_fault_u;

/* 启动注册任务前采集一次，之后由独立的 10 ms 故障任务刷新；主循环调用。 */
void app_fault_update(void);
/* 取得上次采集快照；查询不读取硬件、不改变故障来源状态。 */
app_fault_u app_fault_get(void);
/* 多项故障同时存在时保留全部位，仅返回其中最高优先级。 */
app_fault_priority_e app_fault_highest_priority(app_fault_u faults);

#endif /* APP_FAULT_H */
