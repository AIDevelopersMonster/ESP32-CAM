#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

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

const bool SD_1BIT_MODE = true;
const uint32_t SERIAL_BAUD = 115200;

bool sdMounted = false;
bool cameraReady = false;

void eventLine(const String& payload) {
  Serial.print("SDCLI:");
  Serial.println(payload);
}

String normalizePath(String path) {
  path.trim();
  if (path.length() == 0) return "/";
  if (!path.startsWith("/")) path = "/" + path;
  return path;
}

String cardTypeName(uint8_t type) {
  if (type == CARD_NONE) return "NONE";
  if (type == CARD_MMC) return "MMC";
  if (type == CARD_SD) return "SDSC";
  if (type == CARD_SDHC) return "SDHC";
  return "UNKNOWN";
}

void splitCommand(String line, String& cmd, String& arg1, String& arg2) {
  line.trim();
  int p1 = line.indexOf(';');
  if (p1 < 0) {
    cmd = line;
    arg1 = "";
    arg2 = "";
  } else {
    cmd = line.substring(0, p1);
    int p2 = line.indexOf(';', p1 + 1);
    if (p2 < 0) {
      arg1 = line.substring(p1 + 1);
      arg2 = "";
    } else {
      arg1 = line.substring(p1 + 1, p2);
      arg2 = line.substring(p2 + 1);
    }
  }
  cmd.trim();
  arg1.trim();
  cmd.toUpperCase();
}

void printHelp() {
  Serial.println();
  Serial.println("=== ESP32-CAM UART SD CLI ===");
  Serial.println("Human commands and future GUI commands use the same UART lines.");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  HELP");
  Serial.println("  STATUS");
  Serial.println("  MOUNT");
  Serial.println("  INFO");
  Serial.println("  LS;/");
  Serial.println("  READ;/file.txt");
  Serial.println("  WRITE;/file.txt;text");
  Serial.println("  APPEND;/file.txt;text");
  Serial.println("  MKDIR;/dir");
  Serial.println("  RM;/file.txt;YES");
  Serial.println("  RMDIR;/dir;YES");
  Serial.println("  CAPTURE;/photo.jpg");
  Serial.println();
  Serial.println("Machine-readable lines start with SDCLI: for a later UART GUI.");
  eventLine("READY;FORMAT=CMD;PATH;DATA;COMMANDS=HELP,STATUS,MOUNT,INFO,LS,READ,WRITE,APPEND,MKDIR,RM,RMDIR,CAPTURE");
}

bool mountSD() {
  if (sdMounted) {
    eventLine("OK;CMD=MOUNT;STATUS=ALREADY_MOUNTED");
    return true;
  }

  Serial.println("Mounting SD card in 1-bit SD_MMC mode...");
  if (!SD_MMC.begin("/sdcard", SD_1BIT_MODE)) {
    Serial.println("SD_MMC mount failed");
    eventLine("ERROR;CMD=MOUNT;CODE=MOUNT_FAILED");
    sdMounted = false;
    return false;
  }

  uint8_t type = SD_MMC.cardType();
  if (type == CARD_NONE) {
    Serial.println("No SD card detected");
    eventLine("ERROR;CMD=MOUNT;CODE=NO_CARD");
    sdMounted = false;
    return false;
  }

  sdMounted = true;
  Serial.print("SD mounted. Card type: ");
  Serial.println(cardTypeName(type));
  eventLine(String("OK;CMD=MOUNT;CARD=") + cardTypeName(type));
  return true;
}

bool ensureSD(const String& cmd) {
  if (sdMounted) return true;
  if (mountSD()) return true;
  eventLine(String("ERROR;CMD=") + cmd + ";CODE=SD_NOT_READY");
  return false;
}

void printInfo() {
  if (!ensureSD("INFO")) return;

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  uint64_t total = SD_MMC.totalBytes() / (1024 * 1024);
  uint64_t used = SD_MMC.usedBytes() / (1024 * 1024);

  Serial.println();
  Serial.println("=== SD INFO ===");
  Serial.print("Card type: ");
  Serial.println(cardTypeName(SD_MMC.cardType()));
  Serial.print("Card size MB: ");
  Serial.println(cardSize);
  Serial.print("Total MB: ");
  Serial.println(total);
  Serial.print("Used MB: ");
  Serial.println(used);

  eventLine(String("INFO;CARD=") + cardTypeName(SD_MMC.cardType()) + ";SIZE_MB=" + cardSize + ";TOTAL_MB=" + total + ";USED_MB=" + used);
}

void printStatus() {
  Serial.println();
  Serial.println("=== STATUS ===");
  Serial.print("SD mounted: ");
  Serial.println(sdMounted ? "YES" : "NO");
  Serial.print("Camera ready: ");
  Serial.println(cameraReady ? "YES" : "NO");
  eventLine(String("STATUS;SD=") + (sdMounted ? "READY" : "NOT_READY") + ";CAMERA=" + (cameraReady ? "READY" : "NOT_READY"));
}

void listDir(String path) {
  if (!ensureSD("LS")) return;
  path = normalizePath(path);

  File root = SD_MMC.open(path);
  if (!root) {
    Serial.println("Failed to open directory");
    eventLine(String("ERROR;CMD=LS;CODE=OPEN_FAILED;PATH=") + path);
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Path is not a directory");
    eventLine(String("ERROR;CMD=LS;CODE=NOT_DIR;PATH=") + path);
    root.close();
    return;
  }

  Serial.print("Listing: ");
  Serial.println(path);
  eventLine(String("LS_BEGIN;PATH=") + path);

  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    if (file.isDirectory()) {
      Serial.print("DIR  ");
      Serial.println(name);
      eventLine(String("ITEM;TYPE=DIR;PATH=") + name);
    } else {
      Serial.print("FILE ");
      Serial.print(name);
      Serial.print(" SIZE=");
      Serial.println(file.size());
      eventLine(String("ITEM;TYPE=FILE;PATH=") + name + ";SIZE=" + file.size());
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  eventLine(String("LS_END;PATH=") + path);
}

void readFile(String path) {
  if (!ensureSD("READ")) return;
  path = normalizePath(path);

  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    eventLine(String("ERROR;CMD=READ;CODE=OPEN_FAILED;PATH=") + path);
    return;
  }
  if (file.isDirectory()) {
    Serial.println("Path is a directory, not a file");
    eventLine(String("ERROR;CMD=READ;CODE=IS_DIR;PATH=") + path);
    file.close();
    return;
  }

  eventLine(String("READ_BEGIN;PATH=") + path + ";SIZE=" + file.size());
  Serial.println("----- FILE BEGIN -----");
  while (file.available()) {
    Serial.write(file.read());
  }
  Serial.println();
  Serial.println("----- FILE END -----");
  eventLine(String("READ_END;PATH=") + path);
  file.close();
}

void writeText(String path, String data, bool appendMode) {
  if (!ensureSD(appendMode ? "APPEND" : "WRITE")) return;
  path = normalizePath(path);

  File file = SD_MMC.open(path, appendMode ? FILE_APPEND : FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    eventLine(String("ERROR;CMD=") + (appendMode ? "APPEND" : "WRITE") + ";CODE=OPEN_FAILED;PATH=" + path);
    return;
  }

  size_t written = file.print(data);
  file.close();

  Serial.print(appendMode ? "Appended bytes: " : "Written bytes: ");
  Serial.println(written);
  eventLine(String("OK;CMD=") + (appendMode ? "APPEND" : "WRITE") + ";PATH=" + path + ";BYTES=" + written);
}

void makeDir(String path) {
  if (!ensureSD("MKDIR")) return;
  path = normalizePath(path);
  if (SD_MMC.mkdir(path.c_str())) {
    Serial.println("Directory created");
    eventLine(String("OK;CMD=MKDIR;PATH=") + path);
  } else {
    Serial.println("mkdir failed");
    eventLine(String("ERROR;CMD=MKDIR;CODE=FAILED;PATH=") + path);
  }
}

void removeFile(String path, String confirm) {
  if (!ensureSD("RM")) return;
  path = normalizePath(path);
  confirm.trim();
  confirm.toUpperCase();
  if (confirm != "YES") {
    Serial.println("Confirmation required: RM;/file.txt;YES");
    eventLine(String("ERROR;CMD=RM;CODE=CONFIRM_REQUIRED;PATH=") + path);
    return;
  }

  if (SD_MMC.remove(path.c_str())) {
    Serial.println("File removed");
    eventLine(String("OK;CMD=RM;PATH=") + path);
  } else {
    Serial.println("remove failed");
    eventLine(String("ERROR;CMD=RM;CODE=FAILED;PATH=") + path);
  }
}

void removeDir(String path, String confirm) {
  if (!ensureSD("RMDIR")) return;
  path = normalizePath(path);
  confirm.trim();
  confirm.toUpperCase();
  if (confirm != "YES") {
    Serial.println("Confirmation required: RMDIR;/dir;YES");
    eventLine(String("ERROR;CMD=RMDIR;CODE=CONFIRM_REQUIRED;PATH=") + path);
    return;
  }

  if (SD_MMC.rmdir(path.c_str())) {
    Serial.println("Directory removed");
    eventLine(String("OK;CMD=RMDIR;PATH=") + path);
  } else {
    Serial.println("rmdir failed");
    eventLine(String("ERROR;CMD=RMDIR;CODE=FAILED;PATH=") + path);
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
    eventLine(String("ERROR;CMD=CAMERA;CODE=INIT_FAILED;ESP_ERR=0x") + String((uint32_t)err, HEX));
    cameraReady = false;
    return false;
  }

  Serial.println("Camera initialized");
  eventLine("OK;CMD=CAMERA;STATUS=READY");
  cameraReady = true;
  return true;
}

void captureToSD(String path) {
  if (!ensureSD("CAPTURE")) return;
  if (!cameraReady) {
    Serial.println("Camera is not ready");
    eventLine("ERROR;CMD=CAPTURE;CODE=CAMERA_NOT_READY");
    return;
  }

  path.trim();
  if (path.length() == 0 || path == "/") {
    path = String("/capture_") + millis() + ".jpg";
  }
  path = normalizePath(path);

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    eventLine(String("ERROR;CMD=CAPTURE;CODE=CAPTURE_FAILED;PATH=") + path);
    return;
  }

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for captured image");
    eventLine(String("ERROR;CMD=CAPTURE;CODE=OPEN_FAILED;PATH=") + path);
    esp_camera_fb_return(fb);
    return;
  }

  size_t written = file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  Serial.print("Captured image saved: ");
  Serial.print(path);
  Serial.print(" bytes=");
  Serial.println(written);
  eventLine(String("OK;CMD=CAPTURE;PATH=") + path + ";BYTES=" + written);
}

void handleCommand(String line) {
  String cmd, arg1, arg2;
  splitCommand(line, cmd, arg1, arg2);
  if (cmd.length() == 0) return;

  if (cmd == "HELP") printHelp();
  else if (cmd == "STATUS") printStatus();
  else if (cmd == "MOUNT") mountSD();
  else if (cmd == "INFO") printInfo();
  else if (cmd == "LS") listDir(arg1.length() ? arg1 : "/");
  else if (cmd == "READ") readFile(arg1);
  else if (cmd == "WRITE") writeText(arg1, arg2, false);
  else if (cmd == "APPEND") writeText(arg1, arg2, true);
  else if (cmd == "MKDIR") makeDir(arg1);
  else if (cmd == "RM") removeFile(arg1, arg2);
  else if (cmd == "RMDIR") removeDir(arg1, arg2);
  else if (cmd == "CAPTURE") captureToSD(arg1);
  else {
    Serial.println("Unknown command. Type HELP.");
    eventLine(String("ERROR;CMD=") + cmd + ";CODE=UNKNOWN_COMMAND");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(SERIAL_BAUD);
  Serial.setDebugOutput(false);
  Serial.setTimeout(30000);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-CAM UART SD CLI");
  eventLine("BOOT;NAME=ESP32-CAM_UART_SD_CLI;BAUD=115200");

  initCamera();
  mountSD();
  printHelp();
  printStatus();
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }
  delay(10);
}
