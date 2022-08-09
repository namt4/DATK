#include "input_iot.h"
#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"

input_callback_t input_callback = NULL;
timer_callback_t timer_callback = NULL;
uint64_t start, stop, pressTick;
static TimerHandle_t button_timer;

static void IRAM_ATTR gpio_input_handler(void *arg) //xay ra khi an xuong va tha ra de tru khoang tg cho nhau
{
    uint32_t rtc = xTaskGetTickCountFromISR();
    int gpio_num = (uint32_t)arg;
    if (gpio_get_level(gpio_num) == 0)
    {
        start = rtc;
        xTimerStartFromISR(button_timer, 0);
    }
    else
    {
        xTimerStopFromISR(button_timer, 0);
        stop = rtc;
        pressTick = (stop - start);
        input_callback(gpio_num, pressTick);
    }
}
void vTimerCallback(TimerHandle_t xTimer)
{
    uint32_t ID;
    configASSERT(xTimer);
    ID = (uint32_t)pvTimerGetTimerID(xTimer);
    if (ID == 0)
    {
        timer_callback();
    }
}
void input_io_create(gpio_num_t gpio_num, interrupt_type_edle_t type)
{
    gpio_pad_select_gpio(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(gpio_num, type);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_num, gpio_input_handler, (void *)gpio_num);
    button_timer = xTimerCreate((const char *)"Timer", 3000 / portTICK_RATE_MS, pdFALSE, (void *)0, vTimerCallback);
}
uint8_t input_get_level(gpio_num_t gpio_num)
{
    return gpio_get_level(gpio_num);
}
void input_set_callback(void *cb)
{
    input_callback = cb;
}
void timer_set_callback(void *cb)
{
    timer_callback = cb;
}