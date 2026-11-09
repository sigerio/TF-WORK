#ifndef AUX_OUTPUT_H
#define AUX_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t indicator_enabled;        /* PC3 指示灯输出使能状态。 */
    uint8_t interior_light_enabled;   /* PC4 内部照明输出使能状态。 */
    uint8_t door_heater_enabled;      /* PC5 门加热解冻输出使能状态，不代表解冻完成。 */
} aux_output_status_t;

/* 初始化 PC3、PC4、PC5 输出，并建立默认关闭状态。 */
void aux_output_init(void);

/* 指示灯与内部照明输出使能/关闭。 */
void aux_output_enable_indicator(void);
void aux_output_disable_indicator(void);
void aux_output_enable_interior_light(void);
void aux_output_disable_interior_light(void);

/* 门加热解冻：使能/关闭伴热带供电，恒温由伴热带自身实现。 */
void aux_output_enable_door_heater(void);
void aux_output_disable_door_heater(void);

/* 读取辅助输出的状态快照；status 不可为 NULL。 */
void aux_output_get_status(aux_output_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* AUX_OUTPUT_H */
