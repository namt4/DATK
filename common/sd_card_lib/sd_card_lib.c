#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "sd_card_lib.h"

static const char *TAG = "example";
const char *file_disconnect_wifi_data = MOUNT_POINT "/disconnect_wifi_data.csv";
extern uint32_t id;
sdmmc_card_t *card; // SD/MMC card information
sdmmc_host_t host;

void sd_card_init(void)
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 5000;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    gpio_set_pull_mode(PIN_NUM_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_CS, GPIO_PULLUP_ONLY);

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem."
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    sdmmc_card_print_info(stdout, card);
}

void sd_card_write(module_data_t sensor_data)
{
    FILE *f = fopen(file_disconnect_wifi_data, "a");
    if (f == NULL)
    {
        printf("Khong mo duoc file\n");
    }
    if (id == 0)
    {
        fprintf(f, "%s, %s, %s, %s, %s, %s, %s\n", "ID", "Time", "temperature", "humidity", "pressure", "gas", "uv");
    }
    id++;
    fprintf((FILE *)f, "%d, %s, %.2f, %.2f, %.2f, %.2f, %.2f\n", sensor_data.id, sensor_data.time_values, sensor_data.bme680_values.temperature,
            sensor_data.bme680_values.humidity, sensor_data.bme680_values.pressure, sensor_data.bme680_values.gas_resistance, sensor_data.uv_values);
    fclose(f);
    /* esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    spi_bus_free(host.slot); */
}

module_data_t sd_card_read(uint32_t line_number)
{
    uint32_t i = 0;
    char *token;
    char buffer[1024];
    module_data_t messages_data;
    FILE *f = fopen(file_disconnect_wifi_data, "r");
    while ((fgets((char *)buffer, sizeof(buffer), f)))
    {
        i++;
        if (i == line_number)
        {
            token = strtok(buffer, ",");
            messages_data.id = atoi(token);
            token = strtok(NULL, ",");
            messages_data.time_values = token;
            token = strtok(NULL, ",");
            messages_data.bme680_values.temperature = atof(token);
            token = strtok(NULL, ",");
            messages_data.bme680_values.humidity = atof(token);
            token = strtok(NULL, ",");
            messages_data.bme680_values.pressure = atof(token);
            token = strtok(NULL, ",");
            messages_data.bme680_values.gas_resistance = atof(token);
            token = strtok(NULL, ",");
            messages_data.uv_values = atof(token);
            //printf("data read from sd:%d, %s, %.2f, %.2f, %.2f, %.2f, %.2f\n",messages_data.id,messages_data.time_values,messages_data.bme680_values.temperature,
            //messages_data.bme680_values.humidity,messages_data.bme680_values.temperature,messages_data.bme680_values.gas_resistance,messages_data.uv_values);
        }
    }
    fclose(f);
    /* esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    spi_bus_free(host.slot); */
    return messages_data;
}

char *json_creat(int id, int temperature, int humidity)
{
    static const char *TAG = "CREAT JSON";
    char *json_send_string = NULL;
    cJSON *json = cJSON_CreateObject();
    if (cJSON_AddNumberToObject(json, "ID", id) == NULL)
    {
        goto end;
    }

    if (cJSON_AddNumberToObject(json, "temperature", temperature) == NULL)
    {
        goto end;
    }

    if (cJSON_AddNumberToObject(json, "humidity", humidity) == NULL)
    {
        goto end;
    }

    json_send_string = cJSON_Print(json);
    if (json_send_string == NULL)
    {
        ESP_LOGE(TAG, "JSON creat fail");
    }

end:
    cJSON_Delete(json);
    return json_send_string;
}
