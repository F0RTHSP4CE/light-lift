#ifndef MAGIC_H_INCLUDED
#define MAGIC_H_INCLUDED

#include "esp_system.h"

#define REMOTE_PIN 3
#define LEFT_MOTOR_PIN_A 9
#define LEFT_MOTOR_PIN_B 44
#define RIGHT_MOTOR_PIN_A 7
#define RIGHT_MOTOR_PIN_B 8

#define I2C_MASTER_SDA_IO 2
#define I2C_MASTER_SCL_IO 1
#define I2C_MASTER_FREQ_HZ 100000
#define ACCEL_ADDR_WRITE 0xD0
#define ACCEL_ADDR_READ (ACCEL_ADDR_WRITE | 0x01)

#define REG_ACCEL_XOUT_H 0x3B
#define REG_ACCEL_YOUT_H 0x3D
#define REG_PWR_MGMT_1 0x6B
#define REG_SIGNAL_PATH_RESET 0x68
#define REG_CONFIG 0x1A

#define KEY_STOP 0x0045
#define KEY_UP 0x0009
#define KEY_DOWN 0x0007
#define KEY_TOP 0x0043
#define KEY_BOTTOM 0x0044

#define LEDS_PIN 43
#define LIGHT_PIN 6

#define TOP_ENDSTOP_PIN 4
#define BOTTOM_ENDSTOP_PIN 5

#define WIFI_AP_PASSWORD "1234567890"

// If the expression is not ESP_OK, goto label
#define CHECK_RESULT_L(label, expr)                                            \
  do {                                                                         \
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(expr) != ESP_OK)                         \
      goto label;                                                              \
  } while (0)

// If the expression is not ESP_OK, goto `err` label
#define CHECK_RESULT(expr) CHECK_RESULT_L(err, expr)

#endif