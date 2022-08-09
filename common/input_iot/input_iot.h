#ifndef _INPUT_IOT_H
#define _INPUT_IOT_H

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

typedef void (*input_callback_t)(int, uint64_t);
typedef void (*timer_callback_t)(void);
typedef enum
{
    LO_TO_HI = 1,
    HI_TO_LO = 2,
    ANY_EDLE = 3
} interrupt_type_edle_t;
void input_io_create(gpio_num_t gpio_num, interrupt_type_edle_t type);
uint8_t input_get_level(gpio_num_t gpio_num);
void input_set_callback(void *cb);
void timer_set_callback(void *cb);
#endif
