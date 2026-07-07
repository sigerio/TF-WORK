#ifndef TF_HAL_RCU_H
#define TF_HAL_RCU_H

#include <stdint.h>

void tf_hal_system_clock_config(void);
uint32_t tf_hal_rcu_clock_freq_get(uint32_t clock_id);

#endif /* TF_HAL_RCU_H */
