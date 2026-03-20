#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "protocol_examples_common.h"

#include "freertos/task.h"

#include "drv8835.c"

static const char *MAIN_TAG = "main";

// ----------------------------------------------------------------
// ┏┳┓┏━┓╺┳╸┏━┓┏━┓┏━┓
// ┃┃┃┃ ┃ ┃ ┃ ┃┣┳┛┗━┓
// ╹ ╹┗━┛ ╹ ┗━┛╹┗╸┗━┛
// ----------------------------------------------------------------

#define MOTOR_FRONT_LEFT  DRV8835_MOTOR_A
#define MOTOR_FRONT_RIGHT DRV8835_MOTOR_B
#define MOTOR_REAR_LEFT   DRV8835_MOTOR_A
#define MOTOR_REAR_RIGHT  DRV8835_MOTOR_B

static drv8835_handle_t motor_driver_front;
static drv8835_handle_t motor_driver_rear;

static void motors_init(void) {
  drv8835_config_t config_front = {
      .a_speed_pin   = GPIO_NUM_52,
      .a_dir_pin     = GPIO_NUM_51,
      .b_speed_pin   = GPIO_NUM_31,
      .b_dir_pin     = GPIO_NUM_30,
      .timer         = LEDC_TIMER_0,
      .a_pwm_channel = LEDC_CHANNEL_0,
      .b_pwm_channel = LEDC_CHANNEL_1,
      .a_inverted    = false,
      .b_inverted    = false,
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
      .a_inverted    = false,
      .b_inverted    = false,
  };
  drv8835_init(&config_rear, &motor_driver_rear);

  ESP_LOGI(MAIN_TAG, "Motors initialized");
}

static void set_all_motors(int8_t fl, int8_t fr, int8_t rl, int8_t rr) {
  drv8835_set_motor(motor_driver_front, MOTOR_FRONT_LEFT, fl);
  drv8835_set_motor(motor_driver_front, MOTOR_FRONT_RIGHT, fr);
  drv8835_set_motor(motor_driver_rear, MOTOR_REAR_LEFT, rl);
  drv8835_set_motor(motor_driver_rear, MOTOR_REAR_RIGHT, rr);
}

static void stop_all_motors(void) {
  drv8835_stop_all(motor_driver_front);
  drv8835_stop_all(motor_driver_rear);
}

// ----------------------------------------------------------------
// ╻ ╻╻┏━╸╻         ╻ ╻┏━┓┏┓╻╺┳┓╻  ┏━╸┏━┓
// ┃╻┃┃┣╸ ┃   ╺━╸   ┣━┫┣━┫┃┗┫ ┃┃┃  ┣╸ ┣┳┛
// ┗┻┛╹╹  ╹         ╹ ╹╹ ╹╹ ╹╺┻┛┗━╸┗━╸╹┗╸
// ----------------------------------------------------------------

static esp_err_t echo_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(MAIN_TAG, "WebSocket client connected");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt = {0};
  ws_pkt.type             = HTTPD_WS_TYPE_TEXT;

  // Set max_len = 0 to get the frame len
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(MAIN_TAG, "`httpd_ws_recv_frame` failed to get frame len with %d", ret);
    return ret;
  }

  if (!ws_pkt.len) {
    ESP_LOGI(MAIN_TAG, "Received empty frame");
    return ESP_OK;
  }

  // +1 is for NULL termination as we are expecting a string
  uint8_t *buf = calloc(1, ws_pkt.len + 1);
  if (buf == NULL) {
    ESP_LOGE(MAIN_TAG, "`calloc` memory for buf");
    return ESP_ERR_NO_MEM;
  }
  ws_pkt.payload = buf;

  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    ESP_LOGE(MAIN_TAG, "`httpd_ws_recv_frame` failed with %d", ret);
    free(buf);
    return ret;
  }

  ESP_LOGI(MAIN_TAG, "Type:%d, Message: '%s' (len=%d)", ws_pkt.type, ws_pkt.payload, ws_pkt.len);

  // Parse commands: "FL,FR,RL,RR" (e.g., "75,-50,75,-50")
  // Or simple commands: "stop", "forward", "backward", "left", "right"
  char *cmd          = (char *)ws_pkt.payload;
  char  response[64] = "OK";

  if (strcmp(cmd, "stop") == 0) {

    stop_all_motors();
    strcpy(response, "Motors stopped");

  } else if (strcmp(cmd, "forward") == 0) {

    set_all_motors(75, 75, 75, 75);
    strcpy(response, "Moving forward");

  } else if (strcmp(cmd, "backward") == 0) {

    set_all_motors(-75, -75, -75, -75);
    strcpy(response, "Moving backward");

  } else if (strcmp(cmd, "left") == 0) {

    set_all_motors(-50, 50, -50, 50);
    strcpy(response, "Turning left");

  } else if (strcmp(cmd, "right") == 0) {

    set_all_motors(50, -50, 50, -50);
    strcpy(response, "Turning right");

  } else {

    int fl, fr, rl, rr;
    if (sscanf(cmd, "%d,%d,%d,%d", &fl, &fr, &rl, &rr) == 4) {
      fl = (fl > 100) ? 100 : (fl < -100) ? -100 : fl;
      fr = (fr > 100) ? 100 : (fr < -100) ? -100 : fr;
      rl = (rl > 100) ? 100 : (rl < -100) ? -100 : rl;
      rr = (rr > 100) ? 100 : (rr < -100) ? -100 : rr;
      set_all_motors(fl, fr, rl, rr);
      snprintf(response, sizeof(response), "Set: FL=%d FR=%d RL=%d RR=%d", fl, fr, rl, rr);
    } else {
      strcpy(response, "Unknown command");
    }
  }

  // Send response
  ws_pkt.payload = (uint8_t *)response;
  ws_pkt.len     = strlen(response);
  ret            = httpd_ws_send_frame(req, &ws_pkt);

  if (ret != ESP_OK) ESP_LOGE(MAIN_TAG, "httpd_ws_send_frame failed with %d", ret);
  if (buf) free(buf);
  return ret;
}

// ----------------------------------------------------------------
// ╻ ╻╻┏━╸╻         ┏━┓┏━╸╺┳╸╻ ╻┏━┓
// ┃╻┃┃┣╸ ┃   ╺━╸   ┗━┓┣╸  ┃ ┃ ┃┣━┛
// ┗┻┛╹╹  ╹         ┗━┛┗━╸ ╹ ┗━┛╹
// ----------------------------------------------------------------

static const httpd_uri_t ws = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = echo_handler,
    .user_ctx     = NULL,
    .is_websocket = true,
};

static httpd_handle_t webserver_start() {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  ESP_LOGI(MAIN_TAG, "Starting server on port: '%d'...", config.server_port);

  if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(MAIN_TAG, "Web server started. Registering URI at /ws");
    httpd_register_uri_handler(server, &ws);
    return server;
  }

  ESP_LOGI(MAIN_TAG, "Failed to start web server");
  return NULL;
}

static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server == NULL) {
    ESP_LOGI(MAIN_TAG, "Starting webserver...");
    *server = webserver_start();
  }
}

static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server) {
    ESP_LOGI(MAIN_TAG, "Stopping webserver...");
    if (httpd_stop(*server) == ESP_OK) {
      *server = NULL;
    } else {
      ESP_LOGE(MAIN_TAG, "Failed to stop http server");
    }
  }
}

static esp_err_t wifi_init(httpd_handle_t *server) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, server));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, server));
  return ESP_OK;
  // // Print out Access Point Information
  // wifi_ap_record_t ap_info;
  // ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
  // ESP_LOGI(MAIN_TAG, "--- Access Point Information ---");
  // ESP_LOG_BUFFER_HEX("MAC Address", ap_info.bssid, sizeof(ap_info.bssid));
  // ESP_LOG_BUFFER_CHAR("SSID", ap_info.ssid, sizeof(ap_info.ssid));
  // ESP_LOGI(MAIN_TAG, "Primary Channel: %d", ap_info.primary);
  // ESP_LOGI(MAIN_TAG, "RSSI: %d", ap_info.rssi);
}

// ----------------------------------------------------------------
// ┏┳┓┏━┓╻┏┓╻
// ┃┃┃┣━┫┃┃┗┫
// ╹ ╹╹ ╹╹╹ ╹
// ----------------------------------------------------------------

void app_main(void) {
  // Wifi relies on NVS for storing credentials
  ESP_ERROR_CHECK(nvs_flash_init());

  motors_init();

  // Start WiFi and Web Server
  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(wifi_init(&server));
  server = webserver_start();

  ESP_LOGI(MAIN_TAG, "System ready. Waiting for WebSocket commands...");

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // NOTE: unreachable
  ESP_ERROR_CHECK(example_disconnect());
}
