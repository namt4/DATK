#ifndef _MQTT_APP_H_
#define _MQTT_APP_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*mqtt_handler_t)(char *topic, char *data, int data_len);
void mqtt_app_init(void);
void mqtt_app_start(void);
int mqtt_app_publish(char *topic, char *data, int len, int qos, int retain);
void mqtt_app_subcribe(char *topic, int qos);
void mqtt_set_callback(void *cb);
void mqtt_app_stop(void);
void mqtt_app_disconnect(void);
void mqtt_app_destroy(void);
void mqtt_app_reconnect(void);
void mqtt_app_pub_json(char *time ,float temp, float humidity, float pressure, float gas, float uv);
#endif
