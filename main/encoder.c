#pragma once

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "ENCODER";

typedef struct {
  gpio_num_t pin_a;          // Encoder channel A
  gpio_num_t pin_b;          // Encoder channel B
  int16_t    pulses_per_rev; // Encoder PPR (pulses per revolution)
  int16_t    max_glitch_ns;  // Glitch filter (0 to disable)
} encoder_config_t;

struct encoder_t {
  pcnt_unit_handle_t unit;
  int16_t            pulses_per_rev;
  int32_t            overflow_count; // Track overflows for extended range
};

typedef struct encoder_t *encoder_handle_t;

// Overflow callback to extend count range
static bool overflow_handler(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
  encoder_handle_t encoder = (encoder_handle_t)user_ctx;

  if (edata->watch_point_value > 0) {
    encoder->overflow_count++;
  } else {
    encoder->overflow_count--;
  }
  return false;
}

/**
 * @brief Initialize a quadrature encoder
 */
esp_err_t encoder_init(const encoder_config_t *config, encoder_handle_t *handle) {
  if (!config || !handle) return ESP_ERR_INVALID_ARG;

  encoder_handle_t encoder = calloc(1, sizeof(struct encoder_t));
  if (!encoder) return ESP_ERR_NO_MEM;

  encoder->pulses_per_rev = config->pulses_per_rev;
  encoder->overflow_count = 0;

  // Configure PCNT unit
  pcnt_unit_config_t unit_config = {
      // TODO: tune these limits and document
      .high_limit = 10000,
      .low_limit  = -10000,
  };
  esp_err_t ret = pcnt_new_unit(&unit_config, &encoder->unit);
  if (ret != ESP_OK) {
    free(encoder);
    return ret;
  }

  // Configure glitch filter
  if (config->max_glitch_ns > 0) {
    pcnt_glitch_filter_config_t filter_config = {.max_glitch_ns = config->max_glitch_ns};
    // TODO: handle error (free resources)
    pcnt_unit_set_glitch_filter(encoder->unit, &filter_config);
  }

  // Configure channel A (counts on A edges, direction from B)
  pcnt_chan_config_t chan_a_config = {
      .edge_gpio_num  = config->pin_a,
      .level_gpio_num = config->pin_b,
  };
  pcnt_channel_handle_t chan_a;
  ret = pcnt_new_channel(encoder->unit, &chan_a_config, &chan_a);
  if (ret != ESP_OK) {
    pcnt_del_unit(encoder->unit);
    free(encoder);
    return ret;
  }

  // Configure channel B (counts on B edges, direction from A)
  pcnt_chan_config_t chan_b_config = {
      .edge_gpio_num  = config->pin_b,
      .level_gpio_num = config->pin_a,
  };
  pcnt_channel_handle_t chan_b;
  ret = pcnt_new_channel(encoder->unit, &chan_b_config, &chan_b);
  if (ret != ESP_OK) {
    pcnt_del_unit(encoder->unit);
    free(encoder);
    return ret;
  }

  // TODO: error handling on all of these pcnt_* calls

  // Set up quadrature decoding
  // Channel A: count up on rising edge when B is low, count down when B is high
  pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  // Channel B: count up on rising edge when A is high, count down when A is low
  pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  // Add watch points for overflow handling
  pcnt_unit_add_watch_point(encoder->unit, unit_config.high_limit);
  pcnt_unit_add_watch_point(encoder->unit, unit_config.low_limit);

  // Register overflow callback
  pcnt_event_callbacks_t callbacks = {.on_reach = overflow_handler};
  pcnt_unit_register_event_callbacks(encoder->unit, &callbacks, encoder);

  // Enable and start
  pcnt_unit_enable(encoder->unit);
  pcnt_unit_clear_count(encoder->unit);
  pcnt_unit_start(encoder->unit);

  *handle = encoder;
  ESP_LOGI(TAG, "Encoder initialized on GPIO %d/%d", config->pin_a, config->pin_b);
  return ESP_OK;
}

/**
 * @brief Get raw pulse count (with overflow tracking)
 */
esp_err_t encoder_get_count(encoder_handle_t handle, int32_t *count) {
  if (!handle || !count) return ESP_ERR_INVALID_ARG;

  int current_count;
  pcnt_unit_get_count(handle->unit, &current_count);

  *count = (handle->overflow_count * 10000) + current_count;
  return ESP_OK;
}

/**
 * @brief Get position in degrees
 */
esp_err_t encoder_get_degrees(encoder_handle_t handle, float *degrees) {
  if (!handle || !degrees) return ESP_ERR_INVALID_ARG;

  int32_t count;
  encoder_get_count(handle, &count);

  // 4x counts per pulse in quadrature mode
  *degrees = (count * 360.0f) / (handle->pulses_per_rev * 4);
  return ESP_OK;
}

/**
 * @brief Get position in revolutions
 */
esp_err_t encoder_get_revolutions(encoder_handle_t handle, float *revs) {
  if (!handle || !revs) return ESP_ERR_INVALID_ARG;

  int32_t count;
  encoder_get_count(handle, &count);

  *revs = (float)count / (handle->pulses_per_rev * 4);
  return ESP_OK;
}

/**
 * @brief Reset encoder count to zero
 */
esp_err_t encoder_reset(encoder_handle_t handle) {
  if (!handle) return ESP_ERR_INVALID_ARG;

  handle->overflow_count = 0;
  return pcnt_unit_clear_count(handle->unit);
}

/**
 * @brief Deinitialize encoder
 */
esp_err_t encoder_deinit(encoder_handle_t handle) {
  if (!handle) return ESP_ERR_INVALID_ARG;

  pcnt_unit_stop(handle->unit);
  pcnt_unit_disable(handle->unit);
  pcnt_del_unit(handle->unit);
  free(handle);

  ESP_LOGI(TAG, "Encoder deinitialized");
  return ESP_OK;
}
