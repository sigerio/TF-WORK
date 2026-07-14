#ifndef MODULE_MOTO_HALL_H
#define MODULE_MOTO_HALL_H

#include <stdint.h>

#include "tf_board_config.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODULE_MOTO_HALL_ENABLE
#define MODULE_MOTO_HALL_ENABLE 0U
#endif

typedef enum {
    MODULE_MOTO_HALL_ID_DOOR0 = 0,
    MODULE_MOTO_HALL_ID_DOOR1,

    MODULE_MOTO_HALL_ID_COUNT
} module_moto_hall_id_e;

bool module_moto_hall_init(void);
bool module_moto_hall_is_initialized(void);

uint8_t module_moto_hall_get_level(module_moto_hall_id_e hall_id);
uint32_t module_moto_hall_get_edge_count(module_moto_hall_id_e hall_id);
void module_moto_hall_reset_edge_count(module_moto_hall_id_e hall_id);
void module_moto_hall_reset_all_edge_counts(void);

/* Called only by EXTI10_15_IRQHandler(). */
void module_moto_hall_exti10_15_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MOTO_HALL_H */
