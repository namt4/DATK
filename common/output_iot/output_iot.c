#include "output_iot.h"
#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"

void output_io_create(gpio_num_t gpio_num)
{
    gpio_pad_select_gpio(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
}
void output_set_level(gpio_num_t gpio_num, uint32_t level)
{
    gpio_set_level(gpio_num, level);
}
void output_toggle_pin(gpio_num_t gpio_num)
{
    int old_level = gpio_get_level(gpio_num);
    gpio_set_level(gpio_num, 1 - old_level);
}