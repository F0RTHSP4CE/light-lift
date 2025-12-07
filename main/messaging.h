#ifndef MESSAGING_H_INCLUDED
#define MESSAGING_H_INCLUDED

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

typedef enum {
  MSG_NONE,
  MSG_REMOTE,
  MSG_ENDSTOP,
  MSG_TICK,
  MSG_MQTT,
} message_type_t;

typedef enum {
  CMD_NONE = 0,
  CMD_UP,
  CMD_DOWN,
  CMD_STOP,
  CMD_LIGHT_ON,
  CMD_LIGHT_OFF,
} mqtt_command_t;

typedef struct {
  message_type_t type;
  union {
    struct {
      uint16_t key;
      bool repeat;
    } remote;
    struct {
      uint8_t which : 1;
      uint8_t on : 1;
    } endstop;
    struct {
      mqtt_command_t command;
    } mqtt;
  };
} message_t;

void messaging_init();

extern QueueHandle_t messaging_queue;

#endif