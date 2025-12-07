#include "light.h"
#include "driver/gpio.h"
#include "magic.h"

void light_init() {
  const gpio_config_t config = {
      .pin_bit_mask = (1ULL << LIGHT_PIN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_set_drive_capability(LIGHT_PIN, GPIO_DRIVE_CAP_3));
  light_on_off(0);
}

void light_on_off(uint32_t on) { gpio_set_level(LIGHT_PIN, on); }
