#include "drv8835.c"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
  drv8835_handle_t motor_driver;

  drv8835_config_t config = {.a_phase_gpio = GPIO_NUM_25,
                             .a_enable_gpio = GPIO_NUM_26,
                             .b_phase_gpio = GPIO_NUM_27,
                             .b_enable_gpio = GPIO_NUM_14,
                             .ledc_timer = LEDC_TIMER_0,
                             .ledc_channel_a = LEDC_CHANNEL_0,
                             .ledc_channel_b = LEDC_CHANNEL_1};

  drv8835_init(&config, &motor_driver);

  drv8835_set_motor(motor_driver, DRV8835_MOTOR_A, 75);  // Motor A forward at 75%
  drv8835_set_motor(motor_driver, DRV8835_MOTOR_B, -50); // Motor B reverse at 50%

  vTaskDelay(pdMS_TO_TICKS(2000));

  drv8835_stop_all(motor_driver);
}
