#ifndef TF_HAL_FWDGT_H
#define TF_HAL_FWDGT_H

#include <stdint.h>

void tf_hal_fwdgt_init(void);
void tf_hal_fwdgt_counter_reload(void);
uint32_t tf_hal_fwdgt_flag_get(uint32_t flag);

#endif /* TF_HAL_FWDGT_H */
