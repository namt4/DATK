#include "mqtt_app.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

static const char *TAG = "MQTT_EXAMPLE";
static mqtt_handler_t mqtt_handler = NULL;
static esp_mqtt_client_handle_t client;

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "hieu45678vip/sub", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        mqtt_handler(event->topic, event->data, event->data_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}
void mqtt_app_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://ngoinhaiot.com/",
        .port = 1111,
        .username = "hieu45678vip",
        .password = "hieu45678vip",
        /* .uri = "mqtt://broker.hivemq.com/",
        .port = 1883, */
        .client_id = "esp32_vuminhtuan",
        .event_handle = mqtt_event_handler_cb};
    client = esp_mqtt_client_init(&mqtt_cfg);
}
void mqtt_app_start(void)
{
    esp_mqtt_client_start(client);
}
void mqtt_app_stop(void)
{
    esp_mqtt_client_stop(client);
}
int mqtt_app_publish(char *topic, char *data, int len, int qos, int retain)
{
    return esp_mqtt_client_publish(client, topic, data, len, qos, retain);
}
void mqtt_app_subcribe(char *topic, int qos)
{
    esp_mqtt_client_subscribe(client, topic, qos);
}
void mqtt_app_unsubcribe(char *topic, int qos)
{
    esp_mqtt_client_unsubscribe(client, topic);
}
void mqtt_app_disconnect(void)
{
    esp_mqtt_client_disconnect(client);
}
void mqtt_app_destroy(void)
{
    esp_mqtt_client_destroy(client);
}
void mqtt_app_reconnect(void)
{
    esp_mqtt_client_reconnect(client);
}
void mqtt_set_callback(void *cb)
{
    mqtt_handler = cb;
}
void mqtt_app_pub_json(char *time ,float temp, float humidity, float pressure, float gas, float uv)
{
	char JSON[100];
    char Str_time[100];
	char Str_ND[100];
	char Str_DA[100];
	char Str_PR[100];
	char Str_GA[100];
	char Str_UV[100];

	for(int i = 0 ; i < 100; i++)
	{
		JSON[i] = 0;
        Str_time[i] = 0;
		Str_ND[i] = 0;
		Str_DA[i] = 0;
		Str_PR[i] = 0;
		Str_GA[i] = 0;
		Str_UV[i] = 0;
	}
    sprintf(Str_time, "%s", time);
	sprintf(Str_ND, "%.2f", temp);
	sprintf(Str_DA, "%.2f", humidity);
	sprintf(Str_PR, "%.2f", pressure);
	sprintf(Str_GA, "%.2f", gas);
	sprintf(Str_UV, "%.2f", uv);
	
    strcat(JSON,"{\"TIME\":\"");
	strcat(JSON,Str_time);
	strcat(JSON,"\",");

	strcat(JSON,"\"ND\":\"");
	strcat(JSON,Str_ND);
	strcat(JSON,"\",");
	
	strcat(JSON,"\"DA\":\"");
	strcat(JSON,Str_DA);
	strcat(JSON,"\",");
	
	strcat(JSON,"\"PR\":\"");
	strcat(JSON,Str_PR);
	strcat(JSON,"\",");
	
	strcat(JSON,"\"GA\":\"");
	strcat(JSON,Str_GA);
	strcat(JSON,"\",");
	
	strcat(JSON,"\"UV\":\"");
	strcat(JSON,Str_UV);
	strcat(JSON,"\"}");
	mqtt_app_publish("hieu45678vip/pub",(char *)JSON , strlen(JSON), 1, 0);
    printf("JSON: %s\n", JSON);
}