#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include "Arduino.h"
#include "img_converters.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"

#define CAMERA_MODEL_AI_THINKER
#define PART_BOUNDARY "123456789000000000000987654321"

const char* SETUP_AP_SSID = "ESP32-CAM-SETUP";
const char* SETUP_AP_PASSWORD = "12345678";
const char* MDNS_NAME = "esp32cam";

#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Camera model not selected"
#endif

static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;

String ipUrl() {
  return String("http://") + WiFi.localIP().toString() + String("/");
}

String streamUrl() {
  return String("http://") + WiFi.localIP().toString() + String("/stream");
}

static esp_err_t index_handler(httpd_req_t *req) {
  String html;
  html.reserve(2400);
  html += F("<!DOCTYPE html><html lang='ru'><head>");
  html += F("<meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  html += F("<title>ESP32-CAM</title>");
  html += F("<style>");
  html += F("body{font-family:Arial;text-align:center;background:#111;color:#eee;padding:20px;}");
  html += F(".box{max-width:520px;margin:30px auto;background:#222;padding:22px;border-radius:14px;}");
  html += F(".ip{font-size:20px;color:#7CFC00;word-break:break-all;}");
  html += F("a.button{display:inline-block;margin:10px 0;padding:14px 22px;background:#008cff;color:white;text-decoration:none;border-radius:8px;font-size:18px;}");
  html += F("a.secondary{background:#555;}small{color:#bbb;display:block;margin-top:10px;line-height:1.4;}");
  html += F("</style></head><body><div class='box'>");
  html += F("<h1>ESP32-CAM готова</h1>");
  html += F("<p>IP адрес камеры:</p><p class='ip'>");
  html += ipUrl();
  html += F("</p>");
  html += F("<a class='button' href='/stream'>Открыть видеопоток</a><br>");
  html += F("<a class='button secondary' href='http://esp32cam.local/'>Открыть через esp32cam.local</a>");
  html += F("<small>Если esp32cam.local не открылся, используйте IP адрес из Serial Monitor.</small>");
  html += F("</div></body></html>");

  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, html.c_str(), html.length());
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t jpg_buf_len = 0;
  uint8_t *jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else if (fb->format == PIXFORMAT_JPEG) {
      jpg_buf_len = fb->len;
      jpg_buf = fb->buf;
    } else {
      bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!converted) {
        Serial.println("JPEG compression failed");
        res = ESP_FAIL;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      jpg_buf = NULL;
    } else if (jpg_buf) {
      free(jpg_buf);
      jpg_buf = NULL;
    }

    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    Serial.println("HTTP camera server started");
  } else {
    Serial.println("HTTP camera server start failed");
  }
}

void connectWiFiWithManager() {
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(300);

  // To erase saved Wi-Fi once, uncomment the next line, upload, boot once, then comment it again.
  // wm.resetSettings();

  Serial.println();
  Serial.println("WiFiManager starting");
  Serial.print("Setup AP: ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("Setup AP password: ");
  Serial.println(SETUP_AP_PASSWORD);

  bool connected = wm.autoConnect(SETUP_AP_SSID, SETUP_AP_PASSWORD);
  if (!connected) {
    Serial.println("WiFiManager timeout. Restarting");
    delay(3000);
    ESP.restart();
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void startMDNS() {
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("mDNS started: http://");
    Serial.print(MDNS_NAME);
    Serial.println(".local/");
  } else {
    Serial.println("mDNS failed. Use numeric IP address.");
  }
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    delay(3000);
    ESP.restart();
  }
  Serial.println("Camera initialized");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-CAM WiFiManager Stream");

  connectWiFiWithManager();
  startMDNS();
  initCamera();
  startCameraServer();

  Serial.println();
  Serial.println("Camera page:");
  Serial.println(ipUrl());
  Serial.println("Camera stream:");
  Serial.println(streamUrl());
  Serial.println("mDNS page:");
  Serial.println(String("http://") + MDNS_NAME + String(".local/"));
}

void loop() {
  delay(10);
}
