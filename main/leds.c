#include "driver/gpio.h"
#include "driver/rmt_common.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"

#include "leds.h"
#include "magic.h"

static rmt_encoder_handle_t encoder;
static rmt_channel_handle_t leds_channel;

void leds_init() {
  ESP_ERROR_CHECK(gpio_set_drive_capability(LEDS_PIN, GPIO_DRIVE_CAP_3));
  rmt_bytes_encoder_config_t encoder_config = {
      .bit0 =
          {
              .duration0 = 16,
              .duration1 = 34,
              .level0 = 1,
              .level1 = 0,
          },
      .bit1 =
          {
              .duration0 = 32,
              .duration1 = 18,
              .level0 = 1,
              .level1 = 0,
          },
      .flags =
          {
              .msb_first = 1,
          },
  };
  rmt_tx_channel_config_t channel_config = {
      .gpio_num = LEDS_PIN,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 40000000,
      .mem_block_symbols = 64,
      .intr_priority = 2,
      .flags =
          {
              .with_dma = 0,
          },
      .trans_queue_depth = 10,
  };
  rmt_transmit_config_t tx_config = {
      .flags =
          {
              .eot_level = 0,
              .queue_nonblocking = 1,

          },
      .loop_count = 1,
  };
  vTaskDelay(20);

  uint8_t bytes[9] = {0x3F, 0, 0, 0, 0x3F, 0, 0, 0, 0x3F};
  CHECK_RESULT(rmt_new_tx_channel(&channel_config, &leds_channel));
  CHECK_RESULT(rmt_new_bytes_encoder(&encoder_config, &encoder));
  CHECK_RESULT(rmt_enable(leds_channel));
  CHECK_RESULT(rmt_transmit(leds_channel, encoder, bytes, 9, &tx_config));
err:
}