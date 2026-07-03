/*
  ESP32-CAM — LED HTTP Toggle Server
  ----------------------------------
  Назначение:
    Скетч для ESP32-CAM, который позволяет включать, выключать и переключать
    встроенный светодиод-вспышку на GPIO 4 через HTTP-запросы.

  Для чего нужен:
    Этот скетч работает вместе с Windows GUI-программой esp32cam_led_gui.py.
    GUI отправляет команды на ESP32-CAM по Wi-Fi.

  Логика кнопки:
    - один раз нажали в GUI — LED включился;
    - второй раз нажали — LED выключился;
    - состояние хранится на ESP32-CAM в переменной ledState.

  HTTP-команды:
    http://IP_ESP32_CAM/           — простая web-страница управления;
    http://IP_ESP32_CAM/api/on     — включить LED;
    http://IP_ESP32_CAM/api/off    — выключить LED;
    http://IP_ESP32_CAM/api/toggle — переключить LED;
    http://IP_ESP32_CAM/api/status — получить состояние LED в JSON.

  Плата:
    AI Thinker ESP32-CAM или совместимый ESP32-CAM модуль.

  Важно:
    GPIO 4 на многих ESP32-CAM подключен к яркому светодиоду-вспышке.
    Не смотрите прямо на светодиод при включении.

  Перед загрузкой:
    1. Впишите имя и пароль своего Wi-Fi ниже.
    2. Для прошивки обычно нужно соединить GPIO0 с GND.
    3. После загрузки разомкнуть GPIO0-GND и нажать RST на самой ESP32-CAM.
    4. IP-адрес смотреть в Serial Monitor на скорости 115200.
*/

#include <WiFi.h>
#include <WebServer.h>

// ====== Wi-Fi настройки ======
// Замените значения на имя и пароль своей Wi-Fi сети.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ====== Настройки LED ======
// GPIO 4 на ESP32-CAM обычно подключен к встроенной вспышке.
const int LED_PIN = 4;

// Текущее состояние светодиода: false = выключен, true = включен.
bool ledState = false;

// HTTP-сервер на стандартном порту 80.
WebServer server(80);

void setLed(bool state) {
  ledState = state;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

String ledStatusJson() {
  String json = "{";
  json += "\"led\":";
  json += ledState ? "true" : "false";
  json += ",\"gpio\":";
  json += String(LED_PIN);
  json += "}";
  return json;
}

void sendJsonResponse() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", ledStatusJson());
}

void handleRoot() {
  String html = "";
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32-CAM LED Control</title>";
  html += "<style>body{font-family:Arial;margin:30px;}button{font-size:22px;padding:12px 18px;margin:6px;}</style>";
  html += "</head><body>";
  html += "<h1>ESP32-CAM LED GPIO4</h1>";
  html += "<p>State: <b>";
  html += ledState ? "ON" : "OFF";
  html += "</b></p>";
  html += "<p>";
  html += "<a href='/api/on'><button>ON</button></a>";
  html += "<a href='/api/off'><button>OFF</button></a>";
  html += "<a href='/api/toggle'><button>TOGGLE</button></a>";
  html += "</p>";
  html += "<p><a href='/api/status'>/api/status</a></p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleLedOn() {
  setLed(true);
  sendJsonResponse();
}

void handleLedOff() {
  setLed(false);
  sendJsonResponse();
}

void handleLedToggle() {
  setLed(!ledState);
  sendJsonResponse();
}

void handleLedStatus() {
  sendJsonResponse();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found. Use /api/on, /api/off, /api/toggle or /api/status");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Настраиваем GPIO 4 как выход и сразу выключаем вспышку.
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  Serial.println();
  Serial.println("ESP32-CAM LED HTTP Toggle Server");
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Ждем подключения к Wi-Fi.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected.");
  Serial.print("ESP32-CAM IP address: ");
  Serial.println(WiFi.localIP());

  // Регистрируем HTTP-обработчики.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/on", HTTP_GET, handleLedOn);
  server.on("/api/off", HTTP_GET, handleLedOff);
  server.on("/api/toggle", HTTP_GET, handleLedToggle);
  server.on("/api/status", HTTP_GET, handleLedStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  // Постоянно обслуживаем входящие HTTP-запросы от браузера или Windows GUI.
  server.handleClient();
}
