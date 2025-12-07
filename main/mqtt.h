#ifndef MQTT_H_INCLUDED
#define MQTT_H_INCLUDED

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

extern QueueHandle_t mqtt_queue;

typedef struct {
  bool wifi_event;
  bool on;
} mqtt_shared_message_t;

void mqtt_init();

#endif