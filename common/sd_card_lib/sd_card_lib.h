#ifndef SD_CARD_LIB_H_
#define SD_CARD_LIB_H_
#include "bme680.h"

#define MOUNT_POINT     "/sdcard"
#define PIN_NUM_MISO    19
#define PIN_NUM_MOSI    23
#define PIN_NUM_CLK     18
#define PIN_NUM_CS      5
#define SPI_DMA_CHAN    1


typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t dayWeek;
    uint8_t month;
    uint16_t year;
} ds1302_time_t;

typedef struct {
    int id;
    float uv_values;
    char *time_values;
    bme680_values_float_t bme680_values;
} module_data_t;
void sd_card_init(void);
void sd_card_write(module_data_t);
char* json_creat(int id, int temperature, int humidity);
module_data_t sd_card_read(uint32_t line_number);
#endif
