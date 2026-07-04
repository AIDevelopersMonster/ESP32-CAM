#include "esp_camera.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"

#define PART_BOUNDARY "123456789000000000000987654321"

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

const char* MDNS_NAME = "esp32cam";
const uint32_t WIFI_TIMEOUT_MS = 30000;
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

Preferences prefs;
String savedSsid;
String savedKey;
httpd_handle_t stream_httpd = NULL;

String maskKey(const String& key) {
  if (key.length() == 0) return "<empty>";
  if (key.length() <= 2) return "**";
  return key.substring(0, 1) + "***" + key.substring(key.length() - 1);
}

void loadWiFi() {
  prefs.begin("wifi", false);
  savedSsid = prefs.getString("ssid", "");
  savedKey = prefs.getString("pass", "");
  prefs.end();
}

void saveWiFi(const String& ssid, const String& key) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", key);
  prefs.end();
  savedSsid = ssid;
  savedKey = key;
}

void clearWiFi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  savedSsid = "";
  savedKey = "";
}

void printHelp() {
  Serial.println();
  Serial.println("=== ESP32-CAM Simple CLI Stream ===");
  Serial.println("Enter in Serial Monitor:");
  Serial.println("  SSID;PASS");
  Serial.println("Commands: HELP, SHOW, RESET");
  Serial.println("Line ending: Newline or Both NL & CR");
  Serial.println("After connection open IP URL or http://esp32cam.local/");
}

void printStatus() {
  loadWiFi();
  Serial.println();
  Serial.println("=== STATUS ===");
  Serial.print("Saved SSID: ");
  Serial.println(savedSsid.length() ? savedSsid : "<empty>");
  Serial.print("Saved key: ");
  Serial.println(maskKey(savedKey));
  Serial.print("WiFi status code: ");
  Serial.println(WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("mDNS: http://");
    Serial.print(MDNS_NAME);
    Serial.println(".local/");
  }
}

bool parseLine(String line, String& ssid, String& key) {
  line.trim();
  int sep = line.indexOf(';');
  if (sep < 0) return false;
  ssid = line.substring(0, sep);
  key = line.substring(sep + 1);
  ssid.trim();
  key.trim();
  return ssid.length() > 0 && ssid.length() <= 32 && key.length() <= 64;
}

bool ssidVisible(const String& target) {
  Serial.print("Scanning for SSID: ");
  Serial.println(target);
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  Serial.print("Networks found: ");
  Serial.println(n);
  bool found = false;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(s);
    Serial.print(" RSSI=");
    Serial.println(WiFi.RSSI(i));
    if (s == target) found = true;
  }
  WiFi.scanDelete();
  if (!found) {
    Serial.print("SSID not found: ");
    Serial.println(target);
  }
  return found;
}

bool connectWiFi(const String& ssid, const String& key) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Serial.print("Key length: ");
  Serial.println(key.length());

  WiFi.mode(WIFI_STA);
  if (key.length() == 0) WiFi.begin(ssid.c_str());
  else WiFi.begin(ssid.c_str(), key.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("WiFi connection failed. Status code: ");
  Serial.println(WiFi.status());
  return false;
}

bool readCliCredentials(String& ssid, String& key) {
  printHelp();
  while (true) {
    if (!Serial.available()) {
      delay(50);
      continue;
    }
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.equalsIgnoreCase("HELP")) {
      printHelp();
      continue;
    }
    if (line.equalsIgnoreCase("SHOW")) {
      printStatus();
      continue;
    }
    if (line.equalsIgnoreCase("RESET")) {
      Serial.println("Erasing saved WiFi and restarting...");
      clearWiFi();
      delay(1000);
      ESP.restart();
    }

    if (parseLine(line, ssid, key)) {
      Serial.print("Received SSID: ");
      Serial.println(ssid);
      Serial.print("Received key: ");
      Serial.println(maskKey(key));
      return true;
    }

    Serial.println("Bad format. Use: SSID;PASS");
  }
}

bool setupWiFiCli() {
  loadWiFi();
  if (savedSsid.length() > 0) {
    Serial.print("Saved SSID found: ");
    Serial.println(savedSsid);
    Serial.print("Saved key: ");
    Serial.println(maskKey(savedKey));
    if (ssidVisible(savedSsid) && connectWiFi(savedSsid, savedKey)) return true;
    Serial.println("Saved WiFi did not connect. Enter new data.");
  }

  while (true) {
    String ssid;
    String key;
    readCliCredentials(ssid, key);
    if (!ssidVisible(ssid)) {
      Serial.println("Enter another line: SSID;PASS");
      continue;
    }
    if (connectWiFi(ssid, key)) {
      saveWiFi(ssid, key);
      Serial.println("WiFi settings saved.");
      return true;
    }
    Serial.println("Enter another line: SSID;PASS");
  }
}

bool startMDNS() {
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("mDNS ready: http://");
    Serial.print(MDNS_NAME);
    Serial.println(".local/");
    return true;
  }
  Serial.println("mDNS start failed. Use numeric IP address.");
  return false;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t jpg_len = 0;
  uint8_t *jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else if (fb->format != PIXFORMAT_JPEG) {
      bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!ok) {
        Serial.println("JPEG compression failed");
        res = ESP_FAIL;
      }
    } else {
      jpg_len = fb->len;
      jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, jpg_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    if (fb) esp_camera_fb_return(fb);
    else if (jpg_buf) free(jpg_buf);

    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    Serial.println("HTTP stream server started on /");
  } else {
    Serial.println("HTTP stream server start failed");
  }
}

bool initCamera() {
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
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  Serial.println("Camera initialized");
  return true;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.setTimeout(30000);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-CAM Simple CLI Stream");

  if (!initCamera()) return;
  setupWiFiCli();

  startMDNS();

  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  Serial.print("mDNS URL: http://");
  Serial.print(MDNS_NAME);
  Serial.println(".local/");

  startCameraServer();
}

void loop() {
  delay(1);
}
