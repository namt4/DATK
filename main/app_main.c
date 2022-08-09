#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "string.h"
#include "protocol_examples_common.h"
#include "cJSON.h"

#include "esp_wifi.h"
#include "esp_smartconfig.h"

#include "mqtt_app.h"
#include "input_iot.h"
#include "output_iot.h"
#include "sd_card_lib.h"

#include "bme680.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "ds1302.h"
#include "real_time_app.h"

#include "wifi_manager.h"
// TUAN MODIFY
#define BLINK_GPIO 2
#define CONFIG_CLK_GPIO 12
#define CONFIG_IO_GPIO 13
#define CONFIG_CE_GPIO 14
#define CONFIG_TIMEZONE 7

RTC_DATA_ATTR static int number_of_days = -1;

TaskHandle_t SetClock_Handler;
TaskHandle_t GetClock_Handler;
// TUAN MODIFY
/* ml8511 */
#define DEFAULT_VREF 1100
#define NO_OF_SAMPLES 64

/* bme680 */
#define PORT 0
#define ADDR BME680_I2C_ADDR_1
#define CONFIG_EXAMPLE_I2C_MASTER_SCL 22
#define CONFIG_EXAMPLE_I2C_MASTER_SDA 21

#define Event_press_short (1 << 2)
/* #define Event_press_norrmal (1 << 3) */
#define Event_press_long (1 << 3)

QueueHandle_t uv_queue = NULL;
QueueHandle_t bme680_queue = NULL;
QueueHandle_t sensor_queue = NULL;

/* Declare a variable to hold the created event group. */
EventGroupHandle_t xEventGroup;

static const char *TAG = "https_ota";

#define FIRMWARE_VERSION 0.1
#define UPDATE_JSON_URL "http://datktest123.000webhostapp.com/firmware.json"

// #define ESP_WIFI_SSID "lecocxt"
// #define ESP_WIFI_PASS "0789222879"

#define ESP_WIFI_SSID "Android"
#define ESP_WIFI_PASS "123456788"
#define ESP_MAXIMUM_RETRY 5
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG1 = "wifi_station";

// static int s_retry_num = 0;
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

/* bme680, uv, time */

// receive buffer OTA
char rcv_buffer[200];
#define OTA_URL_SIZE 256

typedef enum
{
    ota = 0,
    wifi_config,
    normal
} wifi_state_t;

static wifi_state_t STATE = normal;

void check_update_task(void *pvParameter);

TaskHandle_t toggle_handler;
TaskHandle_t xwriteHandle = NULL;
TaskHandle_t xreadHandle = NULL;
TaskHandle_t xmqttHandle = NULL;
QueueHandle_t xQueueMQTT = NULL;
typedef struct
{
    char *data_mqtt;
    int data_len;
} xMessageMQTT;

// EventGroupHandle_t sd_card_write_event_group;
uint32_t id = 0;
int32_t temperature = 30, humidity = 90;
extern const char *file_disconnect_wifi_data;
// #define SD_CARD_WRITE_BIT BIT0
// #define SD_CARD_READ_BIT BIT1
void wifi_disconect_write_sd_card_task(void *pvParameters);
void wifi_connect_read_sd_card_task(void *pvParameters);
void toggle_task(void *pvParameters)
{
    vTaskSuspend(NULL);
    for (;;)
    {
        output_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        output_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void pvTaskCode(void *pvParameters)
{
    for (;;)
    {
        EventBits_t uxBits = xEventGroupWaitBits(
            xEventGroup,                          /* The event group being tested. */
            Event_press_short | Event_press_long, /* The bits within the event group to wait for. */
            pdTRUE,                               /* should be cleared before returning. */
            pdFALSE,                              /* Don't wait for both bits, either bit will do. */
            portMAX_DELAY);                       /* Wait a maximum of 100ms for either bit to be set. */

        if (uxBits & Event_press_short)
        {

            printf("Press Short\n");
            STATE = wifi_config;
            /*MQTT*/
            // mqtt_app_destroy();
            change_state_wifi(); // change WM_ORDER_START_AP
            vTaskResume(toggle_handler);
        }
        else if ((uxBits & Event_press_long) != 0)
        {
            printf("Press Long\n");
        }
    }
}

void timer_event_callback()
{
    STATE = ota;
    output_set_level(BLINK_GPIO, 1);
    /*OTA*/
    esp_wifi_set_ps(WIFI_PS_NONE);
    printf("HTTPS OTA, firmware %.1f\n\n", FIRMWARE_VERSION);
    // start the check update task
    xTaskCreate(&check_update_task, "check_update_task", 8192, NULL, 5, NULL);
}
void input_event_callback(int pin, uint64_t pressTick)
{
    if (pin == GPIO_NUM_0)
    {
        int pressTick_ms = pressTick * portTICK_PERIOD_MS;
        /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (pressTick_ms < 1000)
        {
            xEventGroupSetBitsFromISR(xEventGroup, Event_press_short, &xHigherPriorityTaskWoken);
        }
        else if (pressTick_ms > 3000)
        {
            xEventGroupSetBitsFromISR(xEventGroup, Event_press_long, &xHigherPriorityTaskWoken);
        }
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        break;
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            strncpy(rcv_buffer, (char *)evt->data, evt->data_len);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}
// Check update task
// downloads every 30sec the json file with the latest firmware
void check_update_task(void *pvParameter)
{
    while (1)
    {
        printf("Looking for a new firmware...\n");
        // configure the esp_http_client
        esp_http_client_config_t config = {
            .url = UPDATE_JSON_URL,
            .event_handler = _http_event_handler,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        // downloading the json file
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK)
        {
            // parse the json file
            cJSON *json = cJSON_Parse(rcv_buffer);
            if (json == NULL)
                printf("downloaded file is not a valid json, aborting...\n");
            else
            {
                cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
                cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

                // check the version
                if (!cJSON_IsNumber(version))
                    printf("unable to read new version, aborting...\n");
                else
                {

                    double new_version = version->valuedouble;
                    if (new_version > FIRMWARE_VERSION)
                    {

                        printf("current firmware version (%.1f) is lower than the available one (%.1f), upgrading...\n", FIRMWARE_VERSION, new_version);
                        if (cJSON_IsString(file) && (file->valuestring != NULL))
                        {
                            printf("downloading and installing new firmware (%s)...\n", file->valuestring);

                            esp_http_client_config_t ota_client_config = {
                                .url = (const char *)file->valuestring,
                                .cert_pem = (const char *)server_cert_pem_start,
                            };
                            esp_err_t ret = esp_https_ota(&ota_client_config);
                            if (ret == ESP_OK)
                            {
                                printf("OTA OK, restarting...\n");
                                esp_restart();
                            }
                            else
                            {
                                printf("OTA failed...\n");
                            }
                        }
                        else
                            printf("unable to read the new file name, aborting...\n");
                    }
                    else
                        printf("current firmware version (%.1f) is greater or equal to the available one (%.1f), nothing to do...\n", FIRMWARE_VERSION, new_version);
                }
            }
        }
        else
            printf("unable to download the json file, aborting...\n");

        // cleanup closed connecttion
        esp_http_client_cleanup(client);

        printf("\n");
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}
uint8_t state_write = 1;
uint8_t state_wifi = 0;
static void wifi_event_handler(esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // if (STATE == normal)
        // {
        //     if (s_retry_num < 5)
        //     {
        //         esp_wifi_connect();
        //         s_retry_num++;
        //         ESP_LOGI(TAG, "retry to connect to the AP");
        //     }
        // }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // if (STATE == normal)
        // {
        //     if (s_retry_num < ESP_MAXIMUM_RETRY)
        //     {
        //         esp_wifi_connect();
        //         s_retry_num++;
        //         ESP_LOGI(TAG1, "retry to connect to the AP");
        //     }
        //     else
        //     {
        /*  */
        // xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        /* state_write = 1; */
        if (state_write == 1)
        {
            vTaskResume(xwriteHandle);
            vTaskSuspend(xreadHandle);
            state_write += 1;
            printf("resume task write and supend read task\n");
        }
        state_wifi = 0;
        /* printf("SETED WRITE BIT\n"); */
        // }
        ESP_LOGI(TAG1, "connect to the AP fail");
        // }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // ESP_LOGI(TAG1, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        // s_retry_num = 0;

        vTaskSuspend(xwriteHandle);
        vTaskResume(xreadHandle);
        //printf("supend task write and resume read task\n");
        state_write = 1;
        state_wifi = 1;
        // xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        STATE = normal;
        gpio_set_level(BLINK_GPIO, 0);
        vTaskSuspend(toggle_handler);

        // mqtt_app_init();
        // mqtt_app_start();
        // mqtt_app_reconnect();
        /* if (xreadHandle == NULL)
        {
            xTaskCreate(wifi_connect_read_sd_card_task, "wifi_connect_read_sd_card_task", 4096, NULL, 1, &xreadHandle);
            printf("CREATED TASK READ\n");
        } */
        // printf("SETED BIT READ\n");
    }
}

void mqtt_handler(char *topic, char *data, int data_len)
{
    xMessageMQTT mqtt_data = {
        .data_len = data_len,
        .data_mqtt = data};
    xQueueSendFromISR(xQueueMQTT, (void *)&mqtt_data, (TickType_t)0);
}
void mqtt_handler_task(void *pvParameters)
{
    while (1)
    {
        xMessageMQTT mqtt_data;
        if (xQueueReceive(xQueueMQTT, &(mqtt_data), portMAX_DELAY) == pdPASS)
        {
            int len = mqtt_data.data_len;
            char string_json[len + 1];
            strncpy(string_json, mqtt_data.data_mqtt, mqtt_data.data_len);
            strcat(string_json, "\0");
            printf("Data: %s\n", string_json);
            cJSON *json = cJSON_Parse(string_json);
            if (json == NULL)
                printf("receive_data not is json\n");
            else
            {
                cJSON *OTA = cJSON_GetObjectItem(json, "OTA");
                cJSON *Rem = cJSON_GetObjectItem(json, "Rem");
                if (cJSON_IsString(OTA))
                {
                    char *OTA_state = OTA->valuestring;
                    if (strchr(OTA_state, '1'))
                    {
                        xTaskCreate(&check_update_task, "check_update_task", 8192, NULL, 5, NULL);
                    }
                }
                if (cJSON_IsString(Rem))
                {
                    char *Rem_state = Rem->valuestring;
                    if (strchr(Rem_state, '1'))
                    {
                        gpio_set_level(BLINK_GPIO, 1);
                    }
                    else
                    {
                        gpio_set_level(BLINK_GPIO, 0);
                    }
                }
            }
        }
    }
}
void wifi_disconect_write_sd_card_task(void *pvParameters)
{
    vTaskSuspend(NULL);
    printf("DO TASK WRITE\n");
    module_data_t sensor_messages_data_receiver;
    /* if(xreadHandle != NULL) {
        vTaskDelete(xreadHandle);
        printf("DELETED TASK READ\n");
    } */
    for (;;)
    {
        // xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        BaseType_t sensor_Status = xQueueReceive(sensor_queue, &sensor_messages_data_receiver, portMAX_DELAY);
        /* printf("data receiver DS1302 in write task:%s, %.2f, %.2f, %.2f, %.2f, %.2f\n", sensor_messages_data_receiver.time_values, sensor_messages_data_receiver.bme680_values.temperature, sensor_messages_data_receiver.bme680_values.humidity,
                                  sensor_messages_data_receiver.bme680_values.pressure, sensor_messages_data_receiver.bme680_values.gas_resistance, sensor_messages_data_receiver.uv_values); */
        if (sensor_Status == pdPASS)
        {
            //printf("write task receiver from queue\n");
            sd_card_write(sensor_messages_data_receiver);
            printf("WRITEN TO SD CARD\n");
        }
        // xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        // esp_wifi_connect();
        // ESP_LOGI(TAG, "retry to connect to the AP");
        /* xwriteHandle = NULL;
        printf("REMOVED WRITE TASK\n");
        vTaskDelay(1000);
        vTaskDelete(NULL); */
    }
}

void wifi_connect_read_sd_card_task(void *pvParameters)
{
    //mqtt_app_reconnect();
    //printf("DO TASK READ\n");
    module_data_t sensor_messages_data_pub_receiver;
    /* if (xwriteHandle != NULL)
    {
        vTaskDelete(xwriteHandle);
        printf("DELETED TASK WRITE\n");
    } */
    static uint32_t line = 2;
    //printf("ID: %d\n", id);
    for (;;)
    {
        // xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

        struct stat st;
        if (stat(file_disconnect_wifi_data, &st) == 0)
        {
            //printf("file exists\n");
            for (int j = 0; j < id; j++)
            {
                module_data_t messages_send = sd_card_read(line);
                /* char *json_string_send = json_creat(messages_send.id, messages_send.temperature, messages_send.humidity);
                printf("%s\n", json_string_send);
                printf("GIA TRI ID: %d\n", id);
                printf("GIA TRI LINE: %d\n", line);
                int check_publish = mqtt_app_publish("my_topic", json_string_send, 0, 1, 0);
                printf("CHECK_PUBLISH = %d PUBLISHED TO MY_TOPIC\n", check_publish);
                printf("PUBLISHED TO MY_TOPIC\n"); */
                mqtt_app_pub_json(messages_send.time_values, messages_send.bme680_values.temperature, messages_send.bme680_values.humidity,
                                  messages_send.bme680_values.pressure, messages_send.bme680_values.gas_resistance, messages_send.uv_values);
                line++;
            }

            line = 2;
            id = 0;
            unlink(file_disconnect_wifi_data);
            //printf("REMOVED FILE\n");
        }
        else
        {
            //printf("file not exists\n");
            BaseType_t sensor_Status = xQueueReceive(sensor_queue, &sensor_messages_data_pub_receiver, portMAX_DELAY);
            /* printf("data receiver DS1302 in read task:%s, %.2f, %.2f, %.2f, %.2f, %.2f\n", sensor_messages_data_pub_receiver.time_values, sensor_messages_data_pub_receiver.bme680_values.temperature, sensor_messages_data_pub_receiver.bme680_values.humidity,
                                  sensor_messages_data_pub_receiver.bme680_values.pressure, sensor_messages_data_pub_receiver.bme680_values.gas_resistance, sensor_messages_data_pub_receiver.uv_values); */
            if (sensor_Status == pdPASS)
            {
                //printf("read task receiver from queue\n");
                mqtt_app_pub_json(sensor_messages_data_pub_receiver.time_values, sensor_messages_data_pub_receiver.bme680_values.temperature, sensor_messages_data_pub_receiver.bme680_values.humidity,
                                  sensor_messages_data_pub_receiver.bme680_values.pressure, sensor_messages_data_pub_receiver.bme680_values.gas_resistance, sensor_messages_data_pub_receiver.uv_values);
            }
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        /* xreadHandle = NULL;
        printf("REMOVED READ TASK\n");
        vTaskDelete(NULL); */
    }
}
/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
    for (;;)
    {
        ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/**
 * @brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event.
 */
void cb_connection_ok(void *pvParameter)
{
    ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

    /* transform IP to human readable string */
    char str_ip[16];
    esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

    ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);
}

/* void uv_task(void *pvParameter)
{
    float uv_data = 0.25;
    uv_queue = xQueueCreate(5, sizeof(uv_data));
    for (;;) {
        printf("uv_task\n");
        xQueueSend( uv_queue, ( void * )&uv_data, portMAX_DELAY);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
} */

/* void bme680_task(void *pvParameter)
{
    bme680_values_float_t bme680_data = {
            .temperature = 35.45,
            .humidity = 90.08,
            .pressure = 1.62,
            .gas_resistance = 2103.21
    };
    bme680_queue = xQueueCreate(5, sizeof(bme680_data));
    for (;;) {
        printf("bme680_task\n");
        xQueueSend( bme680_queue, ( void * )&bme680_data, portMAX_DELAY);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

} */

/* ml8511 sensor */
static void check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        //printf("eFuse Two Point: Supported\n");
    }
    else
    {
        //printf("eFuse Two Point: NOT supported\n");
    }
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        //printf("eFuse Vref: Supported\n");
    }
    else
    {
       // printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        printf("Characterized using Two Point Value\n");
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        printf("Characterized using eFuse Vref\n");
    }
    else
    {
        printf("Characterized using Default Vref\n");
    }
}
float ml851_uv_intensity(uint32_t voltage)
{
    return ((float)voltage / 1000 - 0.99) * 15 / 1.81;
}
void ml8511_read_task(void *pvParameters)
{
    uint32_t adc_reading = 0;
    static float uv_index;
    static esp_adc_cal_characteristics_t *adc_chars;
    static const adc_channel_t channel = ADC_CHANNEL_6;
    static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
    static const adc_atten_t atten = ADC_ATTEN_DB_11;
    static const adc_unit_t unit = ADC_UNIT_1;
    // Check if Two Point or Vref are burned into eFuse
    //check_efuse();
    // Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);
    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
    /* uv_queue = xQueueCreate(5, sizeof(uv_index)); */

    for (;;)
    {
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            adc_reading += adc1_get_raw((adc1_channel_t)channel);
        }
        adc_reading /= NO_OF_SAMPLES;
        // Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        uv_index = ml851_uv_intensity(voltage);
        /* printf("Raw: %d\tVoltage: %dmV\t uvIntensity: %.2fmW/cm^2\n", adc_reading, voltage, uv_index); */
        /* xQueueSend( uv_queue, ( void * )&uv_index, portMAX_DELAY); */
        xQueueSend(uv_queue, (void *)&uv_index, portMAX_DELAY);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

/* bme680 */
void bme680_test(void *pvParameters)
{
    bme680_t sensor;
    memset(&sensor, 0, sizeof(bme680_t));

    ESP_ERROR_CHECK(bme680_init_desc(&sensor, ADDR, PORT, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));

    // init the sensor
    ESP_ERROR_CHECK(bme680_init_sensor(&sensor));

    // Changes the oversampling rates to 4x oversampling for temperature
    // and 2x oversampling for humidity. Pressure measurement is skipped.
    bme680_set_oversampling_rates(&sensor, BME680_OSR_4X, BME680_OSR_4X, BME680_OSR_2X);

    // Change the IIR filter size for temperature and pressure to 7.
    bme680_set_filter_size(&sensor, BME680_IIR_SIZE_7);

    // Change the heater profile 0 to 200 degree Celsius for 100 ms.
    bme680_set_heater_profile(&sensor, 0, 200, 100);
    bme680_use_heater_profile(&sensor, 0);

    // Set ambient temperature to 10 degree Celsius
    bme680_set_ambient_temperature(&sensor, 25);

    // as long as sensor configuration isn't changed, duration is constant
    uint32_t duration;
    bme680_get_measurement_duration(&sensor, &duration);

    TickType_t last_wakeup = xTaskGetTickCount();

    bme680_values_float_t bme680_data;
    while (1)
    {

        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement(&sensor) == ESP_OK)
        {
            // passive waiting until measurement results are available
            vTaskDelay(duration);

            // get the results and do something with them
            if (bme680_get_results_float(&sensor, &bme680_data) == ESP_OK);
               /*  printf("BME680 Sensor: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm\n",
                       bme680_data.temperature, bme680_data.humidity,
                       bme680_data.pressure, bme680_data.gas_resistance); */
        }
        // passive waiting until 1 second is over
        /* BaseType_t xStatus = xQueueReceive(uv_queue, &sensor_messages_data.uv_values, portMAX_DELAY);
        if ( xStatus == pdPASS ) {
            xQueueSend( sensor_queue, ( void * )&sensor_messages_data, portMAX_DELAY);
        } */
        xQueueSend(bme680_queue, (void *)&bme680_data, portMAX_DELAY);
        /* vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000)); */
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}
void DS1302_setClock(void *pvParameters);
void DS1302_getClock(void *pvParameter)
{

    DS1302_Dev dev;
    DS1302_DateTime dt;

    // Initialize RTC
    ESP_LOGI(TAG, "Start");
    if (!DS1302_begin(&dev, CONFIG_CLK_GPIO, CONFIG_IO_GPIO, CONFIG_CE_GPIO))
    {
        ESP_LOGE(TAG, "Error: DS1302 begin");
        while (1)
        {
            vTaskDelay(1);
        }
    }
    module_data_t sensor_messages_data;
   /*  sensor_queue = xQueueCreate(5, sizeof(sensor_messages_data)); */
    // Initialise the xLastWakeTime variable with the current time.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {

        //printf("DS1302_getClock\n");
        /* Get time date from ds1302 */
        if (!DS1302_getDateTime(&dev, &dt))
        {
            ESP_LOGE(TAG, "Error: DS1302 read failed");
        }
        else
        {
            printf("%d %02d-%02d-%d %d:%02d:%02d\n",
                   dt.dayWeek, dt.dayMonth, dt.month, dt.year, dt.hour, dt.minute, dt.second);
        }
        //printf("state_wifi: %d, number_of_days = %d, dt.dayWeek = %d\n", state_wifi, number_of_days, dt.dayWeek);
        if ((number_of_days == -1 && state_wifi == 1)||(dt.dayWeek != number_of_days  && (state_wifi == 1)))
        {
            xTaskCreate(DS1302_setClock, "DS1302_setClock", 2048, NULL, 1, SetClock_Handler);
            vTaskSuspend(NULL);
            number_of_days = dt.dayWeek;
        }

        BaseType_t uv_Status = xQueueReceive(uv_queue, &sensor_messages_data.uv_values, portMAX_DELAY);
        BaseType_t bme680_Status = xQueueReceive(bme680_queue, &sensor_messages_data.bme680_values, portMAX_DELAY);
        char time_str[26];

        // Get RTC date and time
        

        sprintf(time_str, "%d/%d/%d %d:%d:%d", dt.year, dt.month, dt.dayMonth, dt.hour, dt.minute, dt.second);
        sensor_messages_data.time_values = time_str;
        if (uv_Status == pdPASS && bme680_Status == pdPASS)
        {
            xQueueSend(sensor_queue, (void *)&sensor_messages_data, portMAX_DELAY);
        }
        /* printf("data receiver Sensor: %.2f °C, %.2f %%, %.2f hPa, %.2f Ohm, %.2f mW/cm^2\n",
               sensor_messages_data.bme680_values.temperature, sensor_messages_data.bme680_values.humidity,
               sensor_messages_data.bme680_values.pressure, sensor_messages_data.bme680_values.gas_resistance, sensor_messages_data.uv_values); */
        
        vTaskDelayUntil(&xLastWakeTime, 3000 / portTICK_PERIOD_MS);
    }
}

void DS1302_setClock(void *pvParameters)
{
    printf("DS1302_setClock\n");
    // obtain time over NTP
    ESP_LOGI(TAG, "Connecting to WiFi and getting time over NTP.");
    if (!obtain_time())
    {
        ESP_LOGE(TAG, "Fail to getting time over NTP.");
        while (1)
        {
            vTaskDelay(1);
        }
    }
    // update 'now' variable with current time
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now); //get time
    now = now + (CONFIG_TIMEZONE * 60 * 60);
    localtime_r(&now, &timeinfo); //conver time to format
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

    // Initialize RTC
    DS1302_Dev dev;
    if (!DS1302_begin(&dev, CONFIG_CLK_GPIO, CONFIG_IO_GPIO, CONFIG_CE_GPIO))
    {
        ESP_LOGE(TAG, "Error: DS1302 begin");
        while (1)
        {
            vTaskDelay(1);
        }
    }
    ESP_LOGI(TAG, "Set initial date time...");

    // Set initial date and time
    DS1302_DateTime dt;
    dt.second = timeinfo.tm_sec;
    dt.minute = timeinfo.tm_min;
    dt.hour = timeinfo.tm_hour;
    dt.dayWeek = timeinfo.tm_wday; // 0= Sunday 1 = Monday
    dt.dayMonth = timeinfo.tm_mday;
    dt.month = (timeinfo.tm_mon + 1);
    dt.year = (timeinfo.tm_year + 1900);
    DS1302_setDateTime(&dev, &dt);

    // Check write protect state
    if (DS1302_isWriteProtected(&dev))
    {
        ESP_LOGE(TAG, "Error: DS1302 write protected");
        while (1)
        {
            vTaskDelay(1);
        }
    }

    // Check write protect state
    if (DS1302_isHalted(&dev))
    {
        ESP_LOGE(TAG, "Error: DS1302 halted");
        while (1)
        {
            vTaskDelay(1);
        }
    }
    number_of_days = dt.dayWeek;
    ESP_LOGI(TAG, "Set initial date time done");
    vTaskResume(GetClock_Handler);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Attempt to create the event group. */
    // group event cho button va even cho wifi
    xEventGroup = xEventGroupCreate();
    s_wifi_event_group = xEventGroupCreate();
    xQueueMQTT = xQueueCreate(5, sizeof(xMessageMQTT));
    bme680_queue = xQueueCreate(5, sizeof(bme680_values_float_t));
    uv_queue = xQueueCreate(5, sizeof(float));
    sensor_queue = xQueueCreate(5, sizeof(module_data_t));
    sd_card_init();
    /* if (xwriteHandle == NULL)
    {
        xTaskCreate(wifi_disconect_write_sd_card_task, "wifi_disconect_write_sd_card_task", 4096, NULL, 1, &xwriteHandle);
        printf("CREATED TASK WRITE\n");
    } */
    // vTaskSuspend(xwriteHandle);
    // wifi_init_sta();
    /* start the wifi manager */
    wifi_handler_set_callback(wifi_event_handler); // gan wifi_handler = wifi_event_handler
    wifi_manager_start();

    /* register a callback as an example to how you can in  nager */
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

    /* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
    xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);

    xTaskCreate(pvTaskCode, "task1", 2048, NULL, 1, NULL);
    xTaskCreate(toggle_task, "toggleLed", 512, NULL, 2, &toggle_handler);
    xTaskCreate(mqtt_handler_task, "mqtt_handler_task", 4096, NULL, 2, &xmqttHandle);
    output_io_create(BLINK_GPIO);
    input_set_callback(input_event_callback); // gan input_callback = input_event_callback
    input_io_create(GPIO_NUM_0, ANY_EDLE);    // tao gpio va ngat cho button
    timer_set_callback(timer_event_callback); // gan timer_callback = timer_event_callback
    /*mqtt*/
    mqtt_app_init();
    // mqtt_app_start();
    mqtt_set_callback(mqtt_handler); // gan mqtt_handler(trong mqtt_app.c) = mqtt_handler(trong app_main)
    // mqtt_app_publish("hieu45678vip/pub", "hello", 5, 1, 0);

    if (xwriteHandle == NULL)
    {
        xTaskCreate(wifi_disconect_write_sd_card_task, "wifi_disconect_write_sd_card_task", 4096, NULL, 1, &xwriteHandle);
        //printf("CREATED TASK WRITE\n");
    }
    if (xreadHandle == NULL)
    {
        xTaskCreate(wifi_connect_read_sd_card_task, "wifi_connect_read_sd_card_task", 4096, NULL, 1, &xreadHandle);
        //printf("CREATED TASK READ\n");
    }
    ESP_ERROR_CHECK(i2cdev_init());
    xTaskCreatePinnedToCore(bme680_test, "bme680_test", configMINIMAL_STACK_SIZE * 8, NULL, 1, NULL, 0);

    xTaskCreate(ml8511_read_task, "ml8511_read_task", 2048, NULL, 1, NULL);
    /*  xTaskCreate(bme680_task, "bme680_task", 2048, NULL, 1, NULL); */
    // xTaskCreate(uv_task, "uv_task", 2048, NULL, 1, NULL);
    xTaskCreate(DS1302_getClock, "DS1302_getClock", 4096, NULL, 1, &GetClock_Handler);
}
