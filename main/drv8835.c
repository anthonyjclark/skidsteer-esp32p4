#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DRV8835 operating mode
 */
typedef enum {
  DRV8835_MODE_IN_IN, // IN/IN mode (default): xIN1/xIN2 control direction and
                      // speed
  DRV8835_MODE_PH_EN  // PH/EN mode: xPHASE controls direction, xENABLE controls
                      // speed
} drv8835_mode_t;

/**
 * @brief Motor channel
 */
typedef enum { DRV8835_MOTOR_A = 0, DRV8835_MOTOR_B = 1 } drv8835_motor_t;

/**
 * @brief Motor direction
 */
typedef enum {
  DRV8835_DIR_FORWARD = 0,
  DRV8835_DIR_REVERSE = 1
} drv8835_direction_t;

/**
 * @brief DRV8835 configuration for IN/IN mode
 */
typedef struct {
  gpio_num_t ain1_gpio;
  gpio_num_t ain2_gpio;
  gpio_num_t bin1_gpio;
  gpio_num_t bin2_gpio;
  ledc_timer_t ledc_timer;
  ledc_channel_t ledc_channel_a1;
  ledc_channel_t ledc_channel_a2;
  ledc_channel_t ledc_channel_b1;
  ledc_channel_t ledc_channel_b2;
} drv8835_in_in_config_t;

/**
 * @brief DRV8835 configuration for PH/EN mode
 */
typedef struct {
  gpio_num_t aphase_gpio;
  gpio_num_t aenbl_gpio;
  gpio_num_t bphase_gpio;
  gpio_num_t benbl_gpio;
  ledc_timer_t ledc_timer;
  ledc_channel_t ledc_channel_a;
  ledc_channel_t ledc_channel_b;
} drv8835_ph_en_config_t;

/**
 * @brief DRV8835 handle
 */
typedef struct drv8835_t *drv8835_handle_t;

/**
 * @brief Initialize DRV8835 in IN/IN mode
 */
esp_err_t drv8835_init_in_in(const drv8835_in_in_config_t *config,
                             drv8835_handle_t *handle);

/**
 * @brief Initialize DRV8835 in PH/EN mode
 */
esp_err_t drv8835_init_ph_en(const drv8835_ph_en_config_t *config,
                             drv8835_handle_t *handle);

/**
 * @brief Set motor speed and direction
 * @param handle DRV8835 handle
 * @param motor Motor channel (A or B)
 * @param speed Speed value (-100 to 100). Negative = reverse, positive =
 * forward
 */
esp_err_t drv8835_set_motor(drv8835_handle_t handle, drv8835_motor_t motor,
                            int8_t speed);

/**
 * @brief Set motor speed with explicit direction
 * @param handle DRV8835 handle
 * @param motor Motor channel (A or B)
 * @param direction Forward or reverse
 * @param speed Speed value (0 to 100)
 */
esp_err_t drv8835_set_motor_dir(drv8835_handle_t handle, drv8835_motor_t motor,
                                drv8835_direction_t direction, uint8_t speed);

/**
 * @brief Stop a motor (coast)
 */
esp_err_t drv8835_stop(drv8835_handle_t handle, drv8835_motor_t motor);

/**
 * @brief Brake a motor (short brake)
 */
esp_err_t drv8835_brake(drv8835_handle_t handle, drv8835_motor_t motor);

/**
 * @brief Stop all motors
 */
esp_err_t drv8835_stop_all(drv8835_handle_t handle);

/**
 * @brief Deinitialize DRV8835 and free resources
 */
esp_err_t drv8835_deinit(drv8835_handle_t handle);

#ifdef __cplusplus
}
#endif
