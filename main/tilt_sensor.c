#include "tilt_sensor.h"

#include "driver/i2c.h"
#include "magic.h"

static uint8_t i2c_link_buffer[I2C_LINK_RECOMMENDED_SIZE(10)];

static const uint8_t init_seq[] = {
    REG_PWR_MGMT_1,
    0x88, // reset to default values
    REG_SIGNAL_PATH_RESET,
    7, // reset accelerometer
    REG_PWR_MGMT_1,
    0x08, // SLEEP = 0, TEMP_DIS = 1.
    REG_CONFIG,
    0x04, // DLPF_CFG = 4

    0x00, // end marker (there is no register 0)
};

static bool write_init_sequence() {
  uint8_t const *cmd = init_seq;
  i2c_cmd_handle_t commands = NULL;
  while (*cmd) {
    commands =
        i2c_cmd_link_create_static(i2c_link_buffer, sizeof(i2c_link_buffer));
    CHECK_RESULT(commands != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    CHECK_RESULT(i2c_master_start(commands));
    CHECK_RESULT(i2c_master_write_byte(commands, ACCEL_ADDR_WRITE, true));
    CHECK_RESULT(i2c_master_write_byte(commands, cmd[0], true));
    CHECK_RESULT(i2c_master_write_byte(commands, cmd[1], true));
    CHECK_RESULT(i2c_master_stop(commands));
    CHECK_RESULT(i2c_master_cmd_begin(0, commands, 10));
    cmd += 2;
    i2c_cmd_link_delete_static(commands);
    commands = NULL;
    vTaskDelay(50);
  }
  return true;
err:
  if (commands != NULL)
    i2c_cmd_link_delete_static(commands);
  return false;
}

bool tilt_sensor_init() {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
      .clk_flags = 0,
  };
  CHECK_RESULT(i2c_param_config(I2C_NUM_0, &conf));
  CHECK_RESULT(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
  return write_init_sequence();
err:
  return false;
}

tilt_sensor_reading_t tilt_sensor_read() {
  tilt_sensor_reading_t result;
  union {
    int16_t signed_data;
    uint8_t bytes[2];
  } read_data;
  i2c_cmd_handle_t commands =
      i2c_cmd_link_create_static(i2c_link_buffer, sizeof(i2c_link_buffer));
  CHECK_RESULT(commands != NULL ? ESP_OK : ESP_ERR_NO_MEM);
  CHECK_RESULT(i2c_master_start(commands));
  CHECK_RESULT(i2c_master_write_byte(commands, ACCEL_ADDR_WRITE, true));
  CHECK_RESULT(i2c_master_write_byte(commands, REG_ACCEL_YOUT_H, true));
  CHECK_RESULT(i2c_master_start(commands));
  CHECK_RESULT(i2c_master_write_byte(commands, ACCEL_ADDR_READ, true));
  CHECK_RESULT(
      i2c_master_read(commands, read_data.bytes, 2, I2C_MASTER_LAST_NACK));
  CHECK_RESULT(i2c_master_stop(commands));
  CHECK_RESULT(i2c_master_cmd_begin(I2C_NUM_0, commands, 10));
  result.success = true;
  read_data.bytes[0] = read_data.bytes[0] ^ read_data.bytes[1];
  read_data.bytes[1] = read_data.bytes[1] ^ read_data.bytes[0];
  read_data.bytes[0] = read_data.bytes[0] ^ read_data.bytes[1];
  result.acceleration = read_data.signed_data;
  goto ok;
err:
  result.success = false;
ok:
  if (commands != NULL)
    i2c_cmd_link_delete_static(commands);
  return result;
}