#include "wifi.h"
#include "magic.h"
#include "mqtt.h"

#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "lwip/ip_addr.h"
#include "mqtt.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAXIMUM_RETRY 10

WIFI_CONFIG wifi_config;

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t instance_got_ip;
static esp_event_handler_instance_t instance_any_id;
WIFI_EVENT_ARG event_arg;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < WIFI_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    mqtt_shared_message_t msg;
    msg.wifi_event = true;
    msg.on = true;
    xQueueSend(mqtt_queue, &msg, 10);
  }
}

void start_wifi(WIFI_CONFIG *config) {
  s_wifi_event_group = xEventGroupCreate();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  assert(sta_netif);

  event_arg.cfg = config;
  event_arg.netif = sta_netif;

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, &event_arg,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, &event_arg,
      &instance_got_ip));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  memcpy(wifi_config.sta.ssid, config->ssid, strlen(config->ssid) + 1);
  memcpy(wifi_config.sta.password, config->password,
         strlen(config->password) + 1);

  uint8_t chipid[8];
  ESP_ERROR_CHECK(esp_efuse_mac_get_default(chipid));
  wifi_config_t ap_config = {
      .ap =
          {
              .password = WIFI_AP_PASSWORD,
              .authmode = WIFI_AUTH_WPA2_PSK,
              .beacon_interval = 100,
              .pairwise_cipher = WIFI_CIPHER_TYPE_AES_GMAC128,
              .max_connection = 2,
          },
  };
  sprintf((char *)ap_config.ap.ssid, "LIGHT-%016llX", *((uint64_t *)chipid));

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}
