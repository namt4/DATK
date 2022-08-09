#ifndef _OUTPUT_IOT_H
#define _OUTPUT_IOT_H

#include "esp_err.h"
#include "hal/gpio_types.h"

void output_io_create(gpio_num_t gpio_num);
void output_set_level(gpio_num_t gpio_num, uint32_t level);
void output_toggle_pin(gpio_num_t gpio_num);
#endif
