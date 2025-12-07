#ifndef TILT_SERSOR_H_INCLUDED
#define TILT_SERSOR_H_INCLUDED

#include "esp_system.h"

typedef struct {
  bool success;
  int16_t acceleration;
} tilt_sensor_reading_t;

bool tilt_sensor_init();
tilt_sensor_reading_t tilt_sensor_read();

#endif