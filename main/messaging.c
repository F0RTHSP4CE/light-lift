#include "messaging.h"

QueueHandle_t messaging_queue;

void messaging_init() {
  messaging_queue = xQueueCreate(10, sizeof(message_t));
  assert(messaging_queue);
}
