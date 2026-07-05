#include "Arduino.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "WebServer.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ============================================================
// ESP32-CAM: Take Photo from Web Button and Save to MicroSD
// Board: AI Thinker ESP32-CAM
//
// Features:
// - Wi-Fi access point mode
// - Browser button to capture photo
// - Save JPG files to MicroSD
// - Web gallery
// - View and delete saved photos
// - Web flashlight on/off and brightness slider
//
// Open in browser: http://192.168.4.1
// ============================================================

// ----------------------------
// Wi-Fi AP settings
// ----------------------------
const char* AP_SSID = "ESP32-CAM-PHOTO";
const char* AP_PASS = "12345678";

// ----------------------------
// Web server
// ----------------------------
WebServer server(80);

// ----------------------------
// AI Thinker ESP32-CAM pins
// ----------------------------
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

// ----------------------------
// Flash LED settings
// ----------------------------
// On common AI Thinker ESP32-CAM boards the white flash LED is controlled by GPIO4.
// GPIO4 is also related to SD DATA1 on many boards. The sketch mounts MicroSD in
// 1-bit mode, so this pin can be used for the flash LED.
#define FLASH_LED_GPIO       4
#define FLASH_PWM_CHANNEL    7
#define FLASH_PWM_FREQ       5000
#define FLASH_PWM_RESOLUTION 8

int flashBrightness = 0;
bool flashPwmReady = false;

// ----------------------------
// Flash helpers
// ----------------------------
void writeFlashPwm(uint8_t value) {
  if (!flashPwmReady) {
    digitalWrite(FLASH_LED_GPIO, value > 0 ? HIGH : LOW);
    return;
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(FLASH_LED_GPIO, value);
#else
  ledcWrite(FLASH_PWM_CHANNEL, value);
#endif
}

void setFlashBrightness(int value) {
  if (value < 0) {
    value = 0;
  }

  if (value > 255) {
    value = 255;
  }

  flashBrightness = value;
  writeFlashPwm((uint8_t)flashBrightness);
}

void initFlash() {
  pinMode(FLASH_LED_GPIO, OUTPUT);
  digitalWrite(FLASH_LED_GPIO, LOW);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  flashPwmReady = ledcAttach(FLASH_LED_GPIO, FLASH_PWM_FREQ, FLASH_PWM_RESOLUTION);
#else
  ledcSetup(FLASH_PWM_CHANNEL, FLASH_PWM_FREQ, FLASH_PWM_RESOLUTION);
  ledcAttachPin(FLASH_LED_GPIO, FLASH_PWM_CHANNEL);
  flashPwmReady = true;
#endif

  setFlashBrightness(0);
  Serial.println("Flash LED init OK");
}

// ----------------------------
// HTML helpers
// ----------------------------
String htmlHeader(const String& title) {
  String html;

  html += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + title + "</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;color:#222;}";
  html += ".card{background:white;padding:16px;border-radius:12px;max-width:780px;margin:auto;box-shadow:0 2px 8px #ccc;}";
  html += "button,.btn{display:inline-block;background:#1565c0;color:white;padding:12px 18px;margin:6px 4px;border:0;border-radius:8px;text-decoration:none;font-size:16px;cursor:pointer;}";
  html += ".danger{background:#c62828;}";
  html += ".light{background:#f57c00;}";
  html += ".photo,.panel{border:1px solid #ccc;border-radius:8px;margin:10px 0;padding:10px;background:#fafafa;}";
  html += "img{max-width:100%;height:auto;border-radius:8px;}";
  html += "small{color:#666;}";
  html += "code{background:#eee;padding:2px 4px;border-radius:4px;}";
  html += "input[type=range]{width:100%;max-width:360px;}";
  html += "</style></head><body><div class='card'>";
  html += "<h2>" + title + "</h2>";

  return html;
}

String htmlFooter() {
  return "</div></body></html>";
}

String flashControlsHtml() {
  String html;

  html += "<div class='panel'>";
  html += "<h3>Фонарик</h3>";
  html += "<p>Яркость: <b><span id='flashValue'>" + String(flashBrightness) + "</span></b> / 255</p>";
  html += "<input id='flashSlider' type='range' min='0' max='255' value='" + String(flashBrightness) + "' oninput='setFlash(this.value)'>";
  html += "<p>";
  html += "<button class='light' type='button' onclick='setFlash(180)'>Включить</button>";
  html += "<button type='button' onclick='setFlash(0)'>Выключить</button>";
  html += "</p>";
  html += "<small>На AI Thinker ESP32-CAM фонарик обычно сидит на GPIO4. MicroSD используется в 1-bit mode.</small>";
  html += "</div>";
  html += "<script>";
  html += "function setFlash(v){";
  html += "v=parseInt(v);";
  html += "if(isNaN(v)){v=0;}";
  html += "if(v<0){v=0;}";
  html += "if(v>255){v=255;}";
  html += "var s=document.getElementById('flashSlider');";
  html += "var t=document.getElementById('flashValue');";
  html += "if(s){s.value=v;}";
  html += "if(t){t.textContent=v;}";
  html += "fetch('/flash?value='+v).catch(function(e){});";
  html += "}";
  html += "</script>";

  return html;
}

String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  }

  if (bytes < 1024 * 1024) {
    return String(bytes / 1024.0, 1) + " KB";
  }

  return String(bytes / 1024.0 / 1024.0, 1) + " MB";
}

String safePathFromArg() {
  if (!server.hasArg("name")) {
    return "";
  }

  String name = server.arg("name");

  if (!name.startsWith("/")) {
    name = "/" + name;
  }

  // Minimal protection from path traversal.
  if (name.indexOf("..") >= 0) {
    return "";
  }

  if (!name.endsWith(".jpg") && !name.endsWith(".JPG")) {
    return "";
  }

  return name;
}

String nextPhotoName() {
  for (int i = 1; i < 10000; i++) {
    char path[32];
    snprintf(path, sizeof(path), "/photo_%05d.jpg", i);

    if (!SD_MMC.exists(path)) {
      return String(path);
    }
  }

  char fallback[40];
  snprintf(fallback, sizeof(fallback), "/photo_%lu.jpg", millis());
  return String(fallback);
}

// ----------------------------
// Camera init
// ----------------------------
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

  // Reliable default. Increase later to FRAMESIZE_XGA or FRAMESIZE_UXGA if needed.
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.fb_count = 2;
    config.jpeg_quality = 10;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera init OK");
  return true;
}

// ----------------------------
// MicroSD init
// ----------------------------
bool initSD() {
  // true = 1-bit mode.
  // On ESP32-CAM this is usually safer because it avoids using all SD data lines
  // and reduces conflicts with GPIO4 / LED flash on common boards.
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD_MMC mount failed");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }

  Serial.print("SD card type: ");

  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  Serial.printf("SD total: %llu MB\n", SD_MMC.totalBytes() / (1024 * 1024));
  return true;
}

// ----------------------------
// Capture photo
// ----------------------------
String capturePhotoToSD() {
  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }

  String path = nextPhotoName();
  size_t expectedSize = fb->len;

  File file = SD_MMC.open(path.c_str(), FILE_WRITE);

  if (!file) {
    Serial.println("Failed to open file for writing");
    esp_camera_fb_return(fb);
    return "";
  }

  size_t written = file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  if (written != expectedSize) {
    Serial.println("File write failed");
    SD_MMC.remove(path);
    return "";
  }

  Serial.println("Saved: " + path);
  return path;
}

// ----------------------------
// Web handlers
// ----------------------------
void handleRoot() {
  String html = htmlHeader("ESP32-CAM Photo to MicroSD");

  html += "<p>Нажмите кнопку в браузере — камера сделает фото и сохранит JPG на MicroSD.</p>";
  html += flashControlsHtml();
  html += "<form method='POST' action='/capture'>";
  html += "<button type='submit'>Сделать фото</button>";
  html += "</form>";
  html += "<a class='btn' href='/gallery'>Галерея</a>";
  html += "<hr>";
  html += "<p><small>Wi-Fi AP: <code>ESP32-CAM-PHOTO</code>, password: <code>12345678</code></small></p>";
  html += "<p><small>Open: <code>http://192.168.4.1</code></small></p>";

  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleCapture() {
  String path = capturePhotoToSD();

  if (path == "") {
    String html = htmlHeader("Ошибка");
    html += "<p>Фото не удалось сделать или записать на MicroSD.</p>";
    html += flashControlsHtml();
    html += "<a class='btn' href='/'>Назад</a>";
    html += htmlFooter();

    server.send(500, "text/html", html);
    return;
  }

  String html = htmlHeader("Фото сохранено");

  html += "<p>Сохранено: <b>" + path + "</b></p>";
  html += "<p><img src='/view?name=" + path + "'></p>";
  html += flashControlsHtml();
  html += "<a class='btn' href='/gallery'>Галерея</a>";
  html += "<a class='btn' href='/'>На главную</a>";

  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleGallery() {
  String html = htmlHeader("Галерея MicroSD");

  html += "<a class='btn' href='/'>На главную</a>";

  File root = SD_MMC.open("/");

  if (!root) {
    html += "<p>Не удалось открыть MicroSD.</p>";
    html += htmlFooter();
    server.send(500, "text/html", html);
    return;
  }

  File file = root.openNextFile();
  bool found = false;

  while (file) {
    if (!file.isDirectory()) {
      String path = String(file.name());

      if (!path.startsWith("/")) {
        path = "/" + path;
      }

      if (path.endsWith(".jpg") || path.endsWith(".JPG")) {
        found = true;

        html += "<div class='photo'>";
        html += "<p><b>" + path + "</b><br>";
        html += "<small>" + formatBytes(file.size()) + "</small></p>";
        html += "<p><a href='/view?name=" + path + "' target='_blank'>";
        html += "<img src='/view?name=" + path + "' style='max-width:220px;'>";
        html += "</a></p>";
        html += "<form method='POST' action='/delete' onsubmit=\"return confirm('Удалить " + path + "?');\">";
        html += "<input type='hidden' name='name' value='" + path + "'>";
        html += "<button class='danger' type='submit'>Удалить</button>";
        html += "</form>";
        html += "</div>";
      }
    }

    file = root.openNextFile();
  }

  if (!found) {
    html += "<p>Фото пока нет.</p>";
  }

  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleView() {
  String path = safePathFromArg();

  if (path == "") {
    server.send(400, "text/plain", "Bad file name");
    return;
  }

  if (!SD_MMC.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File file = SD_MMC.open(path, FILE_READ);

  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }

  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleDelete() {
  String path = safePathFromArg();

  if (path == "") {
    server.send(400, "text/plain", "Bad file name");
    return;
  }

  if (!SD_MMC.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  if (SD_MMC.remove(path)) {
    server.sendHeader("Location", "/gallery");
    server.send(303, "text/plain", "");
  } else {
    server.send(500, "text/plain", "Delete failed");
  }
}

void handleFlash() {
  if (server.hasArg("value")) {
    setFlashBrightness(server.arg("value").toInt());
  }

  String json = "{\"flash\":" + String(flashBrightness) + "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ----------------------------
// Setup / loop
// ----------------------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32-CAM Web Photo to MicroSD");

  initFlash();

  if (!initCamera()) {
    Serial.println("Camera failed. Halt.");
    while (true) {
      delay(1000);
    }
  }

  if (!initSD()) {
    Serial.println("SD failed. Halt.");
    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress ip = WiFi.softAPIP();

  Serial.println();
  Serial.println("Wi-Fi AP started");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("PASS: ");
  Serial.println(AP_PASS);
  Serial.print("Open browser: http://");
  Serial.println(ip);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_POST, handleCapture);
  server.on("/gallery", HTTP_GET, handleGallery);
  server.on("/view", HTTP_GET, handleView);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/flash", HTTP_GET, handleFlash);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
