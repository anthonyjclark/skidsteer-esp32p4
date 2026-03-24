#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "encoder.c"

// #define TEST_PIN_A GPIO_NUM_50
// #define TEST_PIN_B GPIO_NUM_49

#define TEST_PIN_A GPIO_NUM_47
#define TEST_PIN_B GPIO_NUM_48

// TODO: test the PPR value
// My toit code has COUNTS-PER-ROTATION := 7 * 2 * 50
#define TEST_PPR 350

// Sample interval in milliseconds
#define SAMPLE_INTERVAL_MS 100

#define VELOCITY_FILTER_ALPHA 0.3f // 0.0-1.0, lower = smoother but slower response
static float filtered_rpm = 0;

void app_main(void) {
  printf("\n=== Single Encoder Test ===\n");
  printf("Pin A: %d, Pin B: %d, PPR: %d\n\n", TEST_PIN_A, TEST_PIN_B, TEST_PPR);

  encoder_handle_t encoder;
  encoder_config_t config = {
      .pin_a          = TEST_PIN_A,
      .pin_b          = TEST_PIN_B,
      .pulses_per_rev = TEST_PPR,
      .max_glitch_ns  = 1000,
  };

  esp_err_t ret = encoder_init(&config, &encoder);
  if (ret != ESP_OK) {
    printf("Failed to init encoder: %s\n", esp_err_to_name(ret));
    return;
  }

  printf("Encoder initialized. Rotate to test.\n\n");

  // Velocity tracking variables
  int32_t prev_count   = 0;
  int64_t prev_time_us = esp_timer_get_time();

  while (true) {
    int32_t count;
    float   degrees, revs;

    encoder_get_count(encoder, &count);
    encoder_get_degrees(encoder, &degrees);
    encoder_get_revolutions(encoder, &revs);

    // Calculate velocity
    int64_t now_us  = esp_timer_get_time();
    int64_t dt_us   = now_us - prev_time_us;
    int32_t d_count = count - prev_count;

    // Counts per second
    float counts_per_sec = (dt_us > 0) ? (d_count * 1000000.0f) / dt_us : 0;

    // Revolutions per second (RPS)
    float rps = counts_per_sec / (TEST_PPR * 4); // 4x for quadrature

    // Revolutions per minute (RPM)
    float rpm    = rps * 60.0f;
    filtered_rpm = (VELOCITY_FILTER_ALPHA * rpm) + ((1.0f - VELOCITY_FILTER_ALPHA) * filtered_rpm);

    // Degrees per second
    float deg_per_sec = rps * 360.0f;

    // Update previous values
    prev_count   = count;
    prev_time_us = now_us;

    // Print all values
    printf("\rCount: %7ld | Deg: %8.1f | RPM: %7.1f (filtered: %7.1f)", count, degrees, rpm, filtered_rpm);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
