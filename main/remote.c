#include "driver/rmt_common.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "magic.h"
#include "messaging.h"
#include "remote.h"

#define TAG "REMOTE"

static const uint32_t LEADER_PULSE[2] = {5400, 12600}; // 9000
static const uint32_t LEADER_PAUSE[2] = {3375, 5625};  // 4500
static const uint32_t REPEAT_PAUSE[2] = {1687, 2812};  // 2250
static const uint32_t DATA_SHORT[2] = {120, 900};      // 560
static const uint32_t DATA_LONG[2] = {901, 2600};      // 1650

rmt_symbol_word_t raw_symbols[100];

static uint16_t last_code = 0;

static rmt_channel_handle_t rmt_channel;
static rmt_receive_config_t rx_config = {
    .signal_range_min_ns = 3186,
    .signal_range_max_ns = 15000000,
    .flags =
        {
            .en_partial_rx = 0,

        },
};

static bool within(uint32_t value, const uint32_t *range) {
  return value >= range[0] && value <= range[1];
}
static bool is_repeat(rmt_symbol_word_t word) {
  return within(word.duration0, LEADER_PULSE) &&
         within(word.duration1, REPEAT_PAUSE);
}

static bool rmt_rx_done_callback(rmt_channel_handle_t rx_chan,
                                 const rmt_rx_done_event_data_t *edata,
                                 void *user_ctx) {
  BaseType_t high_task_wakeup = pdFALSE;
  QueueHandle_t receive_queue = (QueueHandle_t)user_ctx;
  xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
  return high_task_wakeup == pdTRUE;
}

static bool is_leader(rmt_symbol_word_t word) {
  return within(word.duration0, LEADER_PULSE) &&
         within(word.duration1, LEADER_PAUSE);
}

static bool is_zero(rmt_symbol_word_t word) {
  return within(word.duration0, DATA_SHORT) &&
         within(word.duration1, DATA_SHORT);
}

static bool is_one(rmt_symbol_word_t word) {
  return within(word.duration0, DATA_SHORT) &&
         within(word.duration1, DATA_LONG);
}

static rmt_rx_event_callbacks_t callbacks = {
    .on_recv_done = rmt_rx_done_callback,
};

static void remote_task(void *param) {
  QueueHandle_t rx_queue = (QueueHandle_t)param;
  rmt_rx_done_event_data_t rx_data;
  while (1) {
  err:
    // wait for RX done signal
    if (xQueueReceive(rx_queue, &rx_data, pdMS_TO_TICKS(200)) == pdPASS) {
      if (rx_data.num_symbols > 32) {
        union {
          uint32_t bits;
          uint8_t bytes[4];
        } data;
        rmt_symbol_word_t *cur = rx_data.received_symbols;
        if (is_leader(*cur)) {
          bool success = true;
          cur++;
          for (int i = 0; i < 32; i++) {

            if (is_one(*cur))
              data.bits = (data.bits >> 1) | 0x80000000;
            else if (is_zero(*cur))
              data.bits = (data.bits >> 1) & ~0x80000000;
            else {
              success = false;
            }
            cur++;
          }
          if (success && ((data.bytes[0] ^ data.bytes[1]) == 0xFF) &&
              ((data.bytes[2] ^ data.bytes[3]) == 0xFF)) {
            last_code = ((uint16_t)data.bytes[0] << 8) | data.bytes[2];
            ESP_LOGI(TAG, "Remote key code: %04X", last_code);
            message_t msg = {
                .type = MSG_REMOTE,
                .remote =
                    {
                        .key = last_code,
                        .repeat = false,
                    },
            };
            xQueueSend(messaging_queue, &msg, 0);
          } else {
            last_code = 0;
          }
        }
      } else if (last_code && is_repeat(rx_data.received_symbols[0])) {
        message_t msg = {
            .type = MSG_REMOTE,
            .remote =
                {
                    .key = last_code,
                    .repeat = true,
                },
        };
        xQueueSend(messaging_queue, &msg, 0);
      } else {
        last_code = 0;
      }
      CHECK_RESULT(rmt_receive(rmt_channel, raw_symbols, sizeof(raw_symbols),
                               &rx_config));
    } else {
      last_code = 0;
    }
  }
}

void remote_init() {
  rmt_rx_channel_config_t config = {
      .gpio_num = REMOTE_PIN,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,
      .mem_block_symbols = 1200,

      .intr_priority = 2,
      .flags =
          {
              .invert_in = 1,
              .with_dma = 1,
          },
  };
  QueueHandle_t rx_queue = xQueueCreate(10, sizeof(rmt_rx_done_event_data_t));
  assert(rx_queue);

  CHECK_RESULT(rmt_new_rx_channel(&config, &rmt_channel));
  CHECK_RESULT(
      rmt_rx_register_event_callbacks(rmt_channel, &callbacks, rx_queue));
  CHECK_RESULT(rmt_enable(rmt_channel));
  CHECK_RESULT(
      rmt_receive(rmt_channel, raw_symbols, sizeof(raw_symbols), &rx_config));
  TaskHandle_t task_handle = NULL;
  xTaskCreatePinnedToCore(remote_task, "Remote", 8000, rx_queue, 0,
                          &task_handle, 0);
  configASSERT(task_handle);
err:
}