#ifndef LIGHT_OUTPUT_H
#define LIGHT_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t indicator_on;        /* PC3 指示灯当前逻辑状态。 */
    uint8_t interior_light_on;   /* PC4 内部照明当前逻辑状态。 */
    uint8_t decorative_light_on; /* PC5 独立装饰灯带当前逻辑状态。 */
} light_output_status_t;

/* 初始化 PC3、PC4、PC5 输出，并建立默认关闭状态。 */
void light_output_init(void);

/* 通用置位接口：true 点亮，false 关闭。 */
void light_output_set_indicator(bool on);
void light_output_set_interior_light(bool on);
void light_output_set_decorative_light(bool on);

/* 遥控点使用的显式开/关接口。 */
void light_output_set_indicator_on(void);
void light_output_set_indicator_off(void);
void light_output_set_interior_light_on(void);
void light_output_set_interior_light_off(void);
void light_output_set_decorative_light_on(void);
void light_output_set_decorative_light_off(void);
/* 读取三个普通灯输出的状态快照；status 不可为 NULL。 */
void light_output_get_status(light_output_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* LIGHT_OUTPUT_H */
