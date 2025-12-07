#include <esp_log.h>
#include <string.h>

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi.h"

static const char *allowed_settings[] = {
    "ssid",      "password", "mqtt_username", "mqtt_password", "mqtt_host",
    "mqtt_port", NULL,
};

static void save_value(char *name, int name_len, char *value, int value_len) {
  const char *config_name = NULL;
  const char **cur = allowed_settings;
  for (; *cur; cur++) {
    if (!strncmp(*cur, name, name_len)) {
      config_name = *cur;
      break;
    }
  }
  char c = value[value_len];
  value[value_len] = 0;
  nvs_handle_t my_handle;
  nvs_open("storage", NVS_READWRITE, &my_handle);
  ESP_ERROR_CHECK(nvs_set_str(my_handle, config_name, value));
  ESP_ERROR_CHECK(nvs_commit(my_handle));
  nvs_close(my_handle);
  value[value_len] = c;
}

static int hex_decode(char *str) {
  int decoded = 0;
  for (int i = 0; i < 2; i++, str++) {
    decoded <<= 4;
    if (*str >= 'a' && *str <= 'f') {
      decoded |= ((*str - 'a') + 10);
    } else if (*str >= 'A' && *str <= 'F') {
      decoded |= ((*str - 'A') + 10);
    } else if (*str >= '0' && *str <= '9') {
      decoded |= (*str - '0');
    } else {
      return -1;
    }
  }
  return decoded;
}

// Modifies the buffer
static int urldecode(char *buf, int len) {
  char *cur = buf;
  int remain = len;
  char *end = cur + len;
  for (; cur < end; cur++, remain--) {
    if (*cur == '%' && remain > 2) {
      int decoded = hex_decode(cur + 1);
      if (decoded >= 0) {
        *cur = decoded;
        memmove(cur + 1, cur + 3, remain - 3);
        cur += 2;
        remain -= 2;
        len -= 2;
      }
    }
  }
  return len;
}

static esp_err_t update_handler(httpd_req_t *req) {
  char body_buf[129];
  char name[32];
  char value[32];
  int bytes_in_buf = 0;
  bool in_value = false;
  int name_len = 0;
  int value_len = 0;
  for (;;) {
    bytes_in_buf = httpd_req_recv(req, body_buf, sizeof(body_buf) - 1);
    if (bytes_in_buf < 0)
      return bytes_in_buf;
    if (bytes_in_buf == 0)
      break;
    char *cur = body_buf;
    for (; bytes_in_buf; cur++, bytes_in_buf--) {
      char c = *cur;
      if (!in_value && c != '=' && name_len < 32) {
        name[name_len] = c;
        name_len++;
      } else if (!in_value && c == '=') {
        in_value = true;
        value_len = 0;
      } else if (in_value && c != '&' && value_len < 32) {
        value[value_len] = c;
        value_len++;
      } else if (in_value && c == '&') {
        value_len = urldecode(value, value_len);
        save_value(name, name_len, value, value_len);
        name_len = 0;
        value_len = 0;
        in_value = false;
      }
    }
  }
  if (name_len) {
    value_len = urldecode(value, value_len);
    save_value(name, name_len, value, value_len);
  }
  esp_restart();
  return ESP_OK;
}

static void replace_vars(httpd_req_t *req, const char *tpl,
                         const char *tpl_end) {
  for (; tpl < tpl_end;) {
    char *start_pos = strchr(tpl, '$');
    if (start_pos == NULL)
      break;
    httpd_resp_send_chunk(req, tpl, start_pos - tpl);
    start_pos++;
    char *end_pos = start_pos;
    for (; end_pos < tpl_end; end_pos++) {
      char c = *end_pos;
      if ((c < 'a' || c > 'z') && (c != '_'))
        break;
    }
    char *value = "";
    if (!strncmp("ssid", start_pos, end_pos - start_pos)) {
      value = wifi_config.ssid;
    } else if (!strncmp("password", start_pos, end_pos - start_pos)) {
      value = wifi_config.password;
    } else if (!strncmp("mqtt_host", start_pos, end_pos - start_pos)) {
      value = wifi_config.mqtt_host;
    } else if (!strncmp("mqtt_username", start_pos, end_pos - start_pos)) {
      value = wifi_config.mqtt_username;
    } else if (!strncmp("mqtt_password", start_pos, end_pos - start_pos)) {
      value = wifi_config.mqtt_password;
    } else if (!strncmp("mqtt_port", start_pos, end_pos - start_pos)) {
      char buf[16];
      itoa(wifi_config.mqtt_port, buf, 10);
      value = buf;
    }
    if (value && strlen(value)) {
      httpd_resp_sendstr_chunk(req, value);
    }
    tpl = end_pos;
  }
  httpd_resp_send_chunk(req, tpl, tpl_end - tpl);
}

static esp_err_t index_handler(httpd_req_t *req) {
  extern const unsigned char files_index_html_start[] asm(
      "_binary_index_html_start");
  extern const unsigned char files_index_html_end[] asm(
      "_binary_index_html_end");
  httpd_resp_set_type(req, "text/html");
  replace_vars(req, (const char *)files_index_html_start,
               (const char *)files_index_html_end);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static httpd_uri_t index_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
};

static httpd_uri_t update_post = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = update_handler,
};

httpd_handle_t start_webserver(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_handle_t server = NULL;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &index_get);
    httpd_register_uri_handler(server, &update_post);
  }
  return server;
}
