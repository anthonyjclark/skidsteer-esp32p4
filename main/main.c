#include "drv8835.c"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_num.h"
#include <stdio.h>

const gpio_num_t MOTOR_FRONT_LEFT  = DRV8835_MOTOR_A;
const gpio_num_t MOTOR_FRONT_RIGHT = DRV8835_MOTOR_B;
const gpio_num_t MOTOR_REAR_LEFT   = DRV8835_MOTOR_A;
const gpio_num_t MOTOR_REAR_RIGHT  = DRV8835_MOTOR_B;

void app_main(void) {
  drv8835_handle_t motor_driver_front, motor_driver_rear;

  drv8835_config_t config_front = {
      .a_speed_pin   = GPIO_NUM_52,
      .a_dir_pin     = GPIO_NUM_51,
      .b_speed_pin   = GPIO_NUM_31,
      .b_dir_pin     = GPIO_NUM_30,
      .timer         = LEDC_TIMER_0,
      .a_pwm_channel = LEDC_CHANNEL_0,
      .b_pwm_channel = LEDC_CHANNEL_1,
  };
  drv8835_init(&config_front, &motor_driver_front);

  drv8835_config_t config_rear = {
      .a_speed_pin   = GPIO_NUM_20,
      .a_dir_pin     = GPIO_NUM_21,
      .b_speed_pin   = GPIO_NUM_22,
      .b_dir_pin     = GPIO_NUM_23,
      .timer         = LEDC_TIMER_0,
      .a_pwm_channel = LEDC_CHANNEL_2,
      .b_pwm_channel = LEDC_CHANNEL_3,
  };
  drv8835_init(&config_rear, &motor_driver_rear);

  while (true) {
    drv8835_set_motor(motor_driver_front, MOTOR_FRONT_LEFT, 75);
    drv8835_set_motor(motor_driver_front, MOTOR_FRONT_RIGHT, -50);
    drv8835_set_motor(motor_driver_rear, MOTOR_REAR_LEFT, 75);
    drv8835_set_motor(motor_driver_rear, MOTOR_REAR_RIGHT, -50);
    printf("Left motors: Forward 75%%, Right motors: Reverse 50%%\n");

    vTaskDelay(pdMS_TO_TICKS(2000));

    drv8835_stop_all(motor_driver_front);
    drv8835_stop_all(motor_driver_rear);
    printf("Motors stopped\n");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
