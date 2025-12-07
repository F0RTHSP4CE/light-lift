#include "esp_wifi.h"

typedef struct {
  char ssid[32];
  char password[64];
  char mqtt_host[64];
  char mqtt_username[64];
  char mqtt_password[64];
  int mqtt_port;
} WIFI_CONFIG;

typedef struct {
  WIFI_CONFIG *cfg;
  esp_netif_t *netif;
} WIFI_EVENT_ARG;

extern WIFI_CONFIG wifi_config;

void start_wifi(WIFI_CONFIG *config);