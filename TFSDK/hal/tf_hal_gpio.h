#ifndef TF_HAL_GPIO_H
#define TF_HAL_GPIO_H

#include <stdint.h>

void tf_hal_gpio_init(uintptr_t port, uint16_t pin, uint8_t mode, uint8_t pull, uint8_t speed, uint32_t alternate, uint8_t init_state);
void tf_hal_gpio_write(uintptr_t port, uint16_t pin, uint8_t level);
uint8_t tf_hal_gpio_read(uintptr_t port, uint16_t pin);

#endif /* TF_HAL_GPIO_H */
