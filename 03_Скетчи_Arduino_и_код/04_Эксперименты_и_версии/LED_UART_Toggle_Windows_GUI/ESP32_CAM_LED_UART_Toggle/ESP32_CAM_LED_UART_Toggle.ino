/*
  ESP32-CAM — LED UART Toggle
  ---------------------------
  Назначение:
    Скетч для ESP32-CAM, который управляет встроенным светодиодом-вспышкой
    на GPIO 4 через UART / Serial команды.

  Для чего нужен:
    Этот скетч работает вместе с Windows GUI-программой esp32cam_led_uart_gui.py.
    GUI открывает COM-порт USB-TTL адаптера и отправляет текстовые команды.

  Логика кнопки:
    - один раз нажали в GUI — LED включился;
    - второй раз нажали — LED выключился;
    - состояние хранится на ESP32-CAM в переменной ledState.

  UART-команды, которые принимает ESP32-CAM:
    ON      — включить LED;
    OFF     — выключить LED;
    TOGGLE  — переключить LED;
    STATUS  — вернуть текущее состояние;
    HELP    — показать список команд.

  Ответы ESP32-CAM:
    READY:ESP32-CAM_LED_UART
    STATE:ON
    STATE:OFF
    ERROR:UNKNOWN_COMMAND

  Плата:
    AI Thinker ESP32-CAM или совместимый ESP32-CAM модуль.

  Важно:
    GPIO 4 на многих ESP32-CAM подключен к яркому светодиоду-вспышке.
    Не смотрите прямо на светодиод при включении.

  Подключение UART:
    USB-TTL GND -> ESP32-CAM GND
    USB-TTL TX  -> ESP32-CAM U0R / RX0
    USB-TTL RX  -> ESP32-CAM U0T / TX0
    USB-TTL 5V  -> ESP32-CAM 5V, если ваш адаптер стабильно питает плату

  Перед загрузкой:
    1. Для прошивки обычно нужно соединить GPIO0 с GND.
    2. Загрузить скетч через Arduino IDE.
    3. После загрузки разомкнуть GPIO0-GND.
    4. Нажать RST на самой плате ESP32-CAM.
    5. Открыть COM-порт на скорости 115200.
*/

const int LED_PIN = 4;
const unsigned long UART_BAUD = 115200;

bool ledState = false;
String inputLine = "";

void setLed(bool state) {
  ledState = state;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

void printState() {
  Serial.print("STATE:");
  Serial.println(ledState ? "ON" : "OFF");
}

void printHelp() {
  Serial.println("COMMANDS: ON, OFF, TOGGLE, STATUS, HELP");
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  if (command == "ON") {
    setLed(true);
    printState();
  } else if (command == "OFF") {
    setLed(false);
    printState();
  } else if (command == "TOGGLE") {
    setLed(!ledState);
    printState();
  } else if (command == "STATUS") {
    printState();
  } else if (command == "HELP") {
    printHelp();
    printState();
  } else {
    Serial.println("ERROR:UNKNOWN_COMMAND");
    printHelp();
  }
}

void setup() {
  Serial.begin(UART_BAUD);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  Serial.println();
  Serial.println("READY:ESP32-CAM_LED_UART");
  printHelp();
  printState();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') {
      // Игнорируем CR, чтобы одинаково принимать Windows CRLF и обычный LF.
      continue;
    }

    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else {
      inputLine += c;

      // Защита от слишком длинной строки при ошибке на стороне ПК.
      if (inputLine.length() > 64) {
        inputLine = "";
        Serial.println("ERROR:COMMAND_TOO_LONG");
      }
    }
  }
}
