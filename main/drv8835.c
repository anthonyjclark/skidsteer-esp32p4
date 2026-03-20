#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *DRV8835_TAG = "DRV8835";

#define DRV8835_PWM_FREQ_HZ         20000 // 20kHz for quiet operation
#define DRV8835_PWM_RESOLUTION_BITS 8     // 8-bit resolution gives 256 levels of speed control
#define DRV8835_MAX_DUTY            ((1 << DRV8835_PWM_RESOLUTION_BITS) - 1)

typedef enum { DRV8835_MOTOR_A = 0, DRV8835_MOTOR_B = 1 } drv8835_motor_t;
typedef enum { DRV8835_DIR_FORWARD = 0, DRV8835_DIR_REVERSE = 1 } drv8835_direction_t;

// Simplified drive/brake operation with MODE=1 (PHASE/ENABLE)
// +--------+---------+-------+-------+----------------------------------+
// | xPHASE | xENABLE | xOUT1 | xOUT2 | operating mode                   |
// +--------+---------+-------+-------+----------------------------------+
// |   0    |   PWM   |  PWM  |   L   | forward/brake at speed PWM %     |
// |   1    |   PWM   |   L   |  PWM  | reverse/brake at speed PWM %     |
// |   X    |    0    |   L   |   L   | brake low (outputs shorted low)  |
// +--------+---------+-------+-------+----------------------------------+

// In code:
//   enable --> speed
//   phase --> direction

typedef struct {
  gpio_num_t     a_speed_pin;
  gpio_num_t     a_dir_pin;
  gpio_num_t     b_speed_pin;
  gpio_num_t     b_dir_pin;
  ledc_timer_t   timer;
  ledc_channel_t a_pwm_channel;
  ledc_channel_t b_pwm_channel;
  bool           a_inverted;
  bool           b_inverted;
} drv8835_config_t;

struct drv8835_t {
  gpio_num_t     a_speed;
  gpio_num_t     a_dir;
  gpio_num_t     b_speed;
  gpio_num_t     b_dir;
  ledc_timer_t   timer;
  ledc_channel_t a_pwm;
  ledc_channel_t b_pwm;
  bool           a_inverted;
  bool           b_inverted;
};

typedef struct drv8835_t *drv8835_handle_t;

static uint32_t speed_to_duty(uint8_t speed) {
  if (speed > 100) speed = 100;
  return (speed * DRV8835_MAX_DUTY) / 100;
}

/**
 * @brief Initialize DRV8835 in PH/EN mode
 */
esp_err_t drv8835_init(const drv8835_config_t *config, drv8835_handle_t *handle) {
  if (!config || !handle) return ESP_ERR_INVALID_ARG;

  drv8835_handle_t device = calloc(1, sizeof(struct drv8835_t));
  if (!device) return ESP_ERR_NO_MEM;

  device->a_speed    = config->a_speed_pin;
  device->a_dir      = config->a_dir_pin;
  device->b_speed    = config->b_speed_pin;
  device->b_dir      = config->b_dir_pin;
  device->timer      = config->timer;
  device->a_pwm      = config->a_pwm_channel;
  device->b_pwm      = config->b_pwm_channel;
  device->a_inverted = config->a_inverted;
  device->b_inverted = config->b_inverted;

  uint64_t      pin_mask = (1ULL << device->a_dir) | (1ULL << device->b_dir);
  gpio_config_t io_conf  = {
       .pin_bit_mask = pin_mask,
       .mode         = GPIO_MODE_OUTPUT,
       .pull_up_en   = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type    = GPIO_INTR_DISABLE,
  };
  esp_err_t ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    free(device);
    return ret;
  }

  // Configure LEDC timer
  // TODO: will reinitialize if already configured (check first)
  ledc_timer_config_t timer_conf = {
      .speed_mode      = LEDC_LOW_SPEED_MODE,
      .duty_resolution = DRV8835_PWM_RESOLUTION_BITS,
      .timer_num       = device->timer,
      .freq_hz         = DRV8835_PWM_FREQ_HZ,
      .clk_cfg         = LEDC_AUTO_CLK,
  };
  ret = ledc_timer_config(&timer_conf);
  if (ret != ESP_OK) {
    free(device);
    return ret;
  }

  // Configure LEDC channels for speed (enable) pins
  // NOTE: initial duty is 0 (stopped)
  ledc_channel_config_t ch_conf = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_sel  = device->timer,
      .duty       = 0,
      .hpoint     = 0,
  };

  ch_conf.gpio_num = device->a_speed;
  ch_conf.channel  = device->a_pwm;
  ret              = ledc_channel_config(&ch_conf);
  if (ret != ESP_OK) {
    free(device);
    return ret;
  }

  ch_conf.gpio_num = device->b_speed;
  ch_conf.channel  = device->b_pwm;
  ret              = ledc_channel_config(&ch_conf);
  if (ret != ESP_OK) {
    free(device);
    return ret;
  }

  *handle = device;
  ESP_LOGI(DRV8835_TAG, "Initialized (PH/EN mode)");
  return ESP_OK;
}

/**
 * @brief Set motor speed and direction
 */
esp_err_t
drv8835_set_motor_dir(drv8835_handle_t handle, drv8835_motor_t motor, drv8835_direction_t direction, uint8_t speed) {
  if (!handle || motor > DRV8835_MOTOR_B) return ESP_ERR_INVALID_ARG;

  gpio_num_t     dir_pin  = (motor == DRV8835_MOTOR_A) ? handle->a_dir : handle->b_dir;
  ledc_channel_t pwm_ch   = (motor == DRV8835_MOTOR_A) ? handle->a_pwm : handle->b_pwm;
  bool           inverted = (motor == DRV8835_MOTOR_A) ? handle->a_inverted : handle->b_inverted;

  // Apply inversion
  drv8835_direction_t actual_dir = inverted ? !direction : direction;

  gpio_set_level(dir_pin, actual_dir);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, pwm_ch, speed_to_duty(speed));
  ledc_update_duty(LEDC_LOW_SPEED_MODE, pwm_ch);

  ESP_LOGD(
      DRV8835_TAG, "Motor %c: dir=%d (inv=%d), speed=%d", (motor == DRV8835_MOTOR_A) ? 'A' : 'B', actual_dir, inverted,
      speed);
  return ESP_OK;
}

/**
 * @brief Set motor speed and direction
 * @param handle DRV8835 handle
 * @param motor Motor channel (A or B)
 * @param speed Speed value (-100 to 100). Negative = reverse, positive = forward
 */
esp_err_t drv8835_set_motor(drv8835_handle_t handle, drv8835_motor_t motor, int8_t speed) {
  if (!handle) return ESP_ERR_INVALID_ARG;

  drv8835_direction_t dir       = (speed >= 0) ? DRV8835_DIR_FORWARD : DRV8835_DIR_REVERSE;
  uint8_t             abs_speed = (speed >= 0) ? speed : -speed;

  return drv8835_set_motor_dir(handle, motor, dir, abs_speed);
}

/**
 * @brief Stop a motor (coast)
 */
esp_err_t drv8835_stop(drv8835_handle_t handle, drv8835_motor_t motor) { return drv8835_set_motor(handle, motor, 0); }

/**
 * @brief Stop all motors
 */
esp_err_t drv8835_stop_all(drv8835_handle_t handle) {
  esp_err_t ret = drv8835_stop(handle, DRV8835_MOTOR_A);
  ret |= drv8835_stop(handle, DRV8835_MOTOR_B);
  return ret;
}

/**
 * @brief Deinitialize DRV8835 and free resources
 */
esp_err_t drv8835_deinit(drv8835_handle_t handle) {
  if (!handle) return ESP_ERR_INVALID_ARG;

  drv8835_stop_all(handle);
  ledc_stop(LEDC_LOW_SPEED_MODE, handle->a_pwm, 0);
  ledc_stop(LEDC_LOW_SPEED_MODE, handle->b_pwm, 0);

  free(handle);
  ESP_LOGI(DRV8835_TAG, "Deinitialized");
  return ESP_OK;
}
