#ifndef MOTORS_H_INCLUDED
#define MOTORS_H_INCLUDED

#include "esp_system.h"

#define SPEED_MAX 1599

void motors_init();
void motors_set_speed(int16_t left, int16_t right);

#endif