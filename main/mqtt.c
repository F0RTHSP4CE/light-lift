#include "mqtt.h"
#include "magic.h"
#include "messaging.h"
#include "mqtt_client.h"
#include "wifi.h"
#include <esp_log.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static esp_mqtt_client_handle_t client;
QueueHandle_t mqtt_queue;

bool started;
bool connected;

static message_t mqtt_command_message;

static void announce_light_state(bool on) {
  esp_mqtt_client_publish(client, "lightlift/light_state",
                          on ? "light_on" : "light_off", on ? 8 : 9, 0, 1);
}

static int match_command(char *command, int command_len, char *expected) {
  size_t expected_len = strlen(expected);
  if (expected_len != command_len)
    return 0;
  return strncmp(command, expected, expected_len) ? 0 : 1;
}

static mqtt_command_t parse_command(char *command, int command_len) {
  if (match_command(command, command_len, "up"))
    return CMD_UP;
  if (match_command(command, command_len, "down"))
    return CMD_DOWN;
  if (match_command(command, command_len, "stop"))
    return CMD_STOP;
  if (match_command(command, command_len, "light_on"))
    return CMD_LIGHT_ON;
  if (match_command(command, command_len, "light_off"))
    return CMD_LIGHT_OFF;
  return CMD_NONE;
}

static void mqtt_onconnect() {
  if (!started) {
    CHECK_RESULT(esp_mqtt_client_start(client));
    started = true;
  }
err:
}

static void pass_command(mqtt_command_t command) {
  mqtt_command_message.type = MSG_MQTT;
  mqtt_command_message.mqtt.command = command;
  xQueueSend(messaging_queue, &mqtt_command_message, 0);
  if (command == CMD_LIGHT_ON || command == CMD_LIGHT_OFF) {
    announce_light_state(command == CMD_LIGHT_ON);
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  extern const char config_payload_start[] asm(
      "_binary_config_payload_json_start");
  extern const char config_payload_end[] asm("_binary_config_payload_json_end");
  esp_mqtt_event_t *event = (esp_mqtt_event_t *)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    esp_mqtt_client_subscribe_single(client, "lightlift/command", 1);
    esp_mqtt_client_publish(
        client, "homeassistant/device/light_lift/config", config_payload_start,
        config_payload_end - config_payload_start - 1, 0, 1);
    break;
  case MQTT_EVENT_DATA:
    mqtt_command_t mqtt_command = parse_command(event->data, event->data_len);
    if (mqtt_command != CMD_NONE)
      pass_command(mqtt_command);
  default:
    break;
  }
}

static void mqtt_task(void *param) {
  QueueHandle_t queue = (QueueHandle_t)param;
  mqtt_shared_message_t msg;
  while (1) {
    if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(150)) == pdPASS) {
      if (msg.wifi_event) {
        if (msg.on) {
          mqtt_onconnect();
        }
      } else {
        announce_light_state(msg.on);
      }
    }
  }
}

void mqtt_init() {
  mqtt_queue = xQueueCreate(4, sizeof(mqtt_shared_message_t));
  assert(mqtt_queue);
  esp_mqtt_client_config_t config = {
      .broker =
          {
              .address =
                  {
                      .hostname = wifi_config.mqtt_host,
                      .port = wifi_config.mqtt_port,
                      .transport = MQTT_TRANSPORT_OVER_TCP,
                  },
          },
      .buffer =
          {
              .size = 4096,
          },
      .credentials =
          {
              .client_id = "LIGHT_LIFT",
              .username = wifi_config.mqtt_username,
              .authentication =
                  {
                      .password = wifi_config.mqtt_password,
                  },
          },
  };
  client = esp_mqtt_client_init(&config);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                 NULL);

  TaskHandle_t task_handle = NULL;
  xTaskCreatePinnedToCore(mqtt_task, "MQTT", 8000, mqtt_queue, 1, &task_handle,
                          0);
  configASSERT(task_handle);
}