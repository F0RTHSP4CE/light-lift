#include "bdc_motor.h"
#include "driver/gpio.h"
#include "driver/mcpwm_gen.h"
#include "driver/mcpwm_timer.h"
#include "esp_system.h"

#include "magic.h"
#include "motors.h"

bdc_motor_handle_t left_motor;
bdc_motor_handle_t right_motor;

static void motor_init(bdc_motor_handle_t *motor, uint32_t pina, uint32_t pinb,
                       uint32_t group_id) {
  gpio_set_direction(TOP_ENDSTOP_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(BOTTOM_ENDSTOP_PIN, GPIO_MODE_INPUT);

  bdc_motor_config_t motor_config = {
      .pwm_freq_hz = 10000,
      .pwma_gpio_num = pina,
      .pwmb_gpio_num = pinb,
  };
  bdc_motor_mcpwm_config_t mcpwm_config = {
      .group_id = group_id,
      .resolution_hz = 16000000,
  };
  CHECK_RESULT(bdc_motor_new_mcpwm_device(&motor_config, &mcpwm_config, motor));
  CHECK_RESULT(bdc_motor_enable(*motor));
  CHECK_RESULT(bdc_motor_brake(*motor));
err:
}

void motors_init() {
  motor_init(&left_motor, LEFT_MOTOR_PIN_A, LEFT_MOTOR_PIN_B, 0);
  motor_init(&right_motor, RIGHT_MOTOR_PIN_A, RIGHT_MOTOR_PIN_B, 1);
}

static void motor_set_speed(bdc_motor_handle_t motor, int16_t speed) {
  if (speed != 0) {
    if (speed < 0) {
      speed = -speed;
      bdc_motor_reverse(motor);
    } else {
      bdc_motor_forward(motor);
    }
    bdc_motor_set_speed(motor, speed);
  } else {
    bdc_motor_brake(motor);
  }
}

void motors_set_speed(int16_t left, int16_t right) {
  motor_set_speed(left_motor, left);
  motor_set_speed(right_motor, right);
}