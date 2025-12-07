#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include <string.h>

#include "http_server.h"
#include "leds.h"
#include "light.h"
#include "magic.h"
#include "messaging.h"
#include "motors.h"
#include "mqtt.h"
#include "remote.h"
#include "tilt_sensor.h"
#include "wifi.h"

static TimerHandle_t timer;

typedef enum {
  ST_IDLE,
  ST_UP,
  ST_DOWN,
  ST_TOP,
  ST_BOTTOM,
} system_state_t;

struct {
  uint32_t timer_ticks;
  uint32_t light_on;
  system_state_t state;
} state;

#define READ_VALUE(name)                                                       \
  do {                                                                         \
    value_size = sizeof(wifi_config.name);                                     \
    nvs_get_str(my_handle, #name, wifi_config.name, &value_size);              \
  } while (0)

#define READ_INT_VALUE(name)                                                   \
  do {                                                                         \
    char buf[128];                                                             \
    size_t size = 127;                                                         \
    nvs_get_str(my_handle, #name, buf, &size);                                 \
    wifi_config.name = atoi(buf);                                              \
  } while (0)

static void read_config() {
  ESP_ERROR_CHECK(nvs_flash_init());
  nvs_handle_t my_handle;
  size_t value_size;
  nvs_open("storage", NVS_READWRITE, &my_handle);
  READ_VALUE(ssid);
  READ_VALUE(password);
  READ_VALUE(mqtt_host);
  READ_VALUE(mqtt_username);
  READ_VALUE(mqtt_password);
  READ_INT_VALUE(mqtt_port);
  nvs_close(my_handle);
}

static void timer_func(TimerHandle_t xTimer) {
  message_t msg = {.type = MSG_TICK};
  xQueueSend(messaging_queue, &msg, 0);
}

static void restart_timer() { state.timer_ticks = 0; }

static void update_light() { light_on_off(state.light_on); }

static void update_motors() {
  switch (state.state) {
  case ST_UP:
  case ST_TOP:
    motors_set_speed(SPEED_MAX, SPEED_MAX);
    break;
  case ST_DOWN:
  case ST_BOTTOM:
    motors_set_speed(-SPEED_MAX, -SPEED_MAX);
    break;

  default:
    motors_set_speed(0, 0);
    break;
    break;
  }
}

static void process_timer() {
  if (state.timer_ticks > 10) {
    if (state.state != ST_BOTTOM && state.state != ST_TOP) {
      state.state = ST_IDLE;
    }
    state.timer_ticks = 0;
  } else {
    state.timer_ticks++;
  }
}

void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  light_init();
  light_on_off(0);
  messaging_init();
  motors_init();
  // tilt_sensor_init(); // Not used
  remote_init();
  read_config();
  mqtt_init();
  start_wifi(&wifi_config);
  start_webserver();
  // leds_init(); // Not used

  timer = xTimerCreate("tick timer", 1, pdTRUE, NULL, timer_func);
  assert(timer);
  xTimerStart(timer, 100);

  for (;;) {
    message_t msg;
    if (xQueueReceive(messaging_queue, &msg, 100) == pdTRUE) {
      switch (msg.type) {
      case MSG_TICK:
        process_timer();
        break;
      case MSG_MQTT:
        if (msg.mqtt.command == CMD_LIGHT_ON)
          state.light_on = 1;
        if (msg.mqtt.command == CMD_LIGHT_OFF)
          state.light_on = 0;
        if (msg.mqtt.command == CMD_UP)
          state.state = ST_TOP;
        if (msg.mqtt.command == CMD_DOWN)
          state.state = ST_BOTTOM;
        if (msg.mqtt.command == CMD_STOP)
          state.state = ST_IDLE;
        break;
      case MSG_REMOTE:
        if (msg.remote.key == KEY_UP) {
          state.state = ST_UP;
        }
        if (msg.remote.key == KEY_DOWN) {
          state.state = ST_DOWN;
        }
        if (msg.remote.key == KEY_TOP) {
          state.state = ST_TOP;
        }
        if (msg.remote.key == KEY_BOTTOM) {
          state.state = ST_BOTTOM;
        }
        if (msg.remote.key == KEY_STOP) {
          if (state.state == ST_IDLE && !msg.remote.repeat) {
            state.light_on = !state.light_on;
            mqtt_shared_message_t msg;
            msg.wifi_event = false;
            msg.on = state.light_on;
            xQueueSend(mqtt_queue, &msg, 10);
          }
          state.state = ST_IDLE;
        }
        restart_timer();
        break;

      default:
        break;
      }
      if ((state.state == ST_UP || state.state == ST_TOP) &&
          gpio_get_level(TOP_ENDSTOP_PIN)) {
        state.state = ST_IDLE;
      }
      if ((state.state == ST_DOWN || state.state == ST_BOTTOM) &&
          gpio_get_level(BOTTOM_ENDSTOP_PIN)) {
        state.state = ST_IDLE;
      }
      update_motors();
      update_light();
    }
  }
}
