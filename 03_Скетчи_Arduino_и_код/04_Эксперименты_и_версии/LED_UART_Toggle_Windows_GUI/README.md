# LED UART Toggle + Windows GUI

Проект для управления встроенным светодиодом-вспышкой ESP32-CAM на GPIO 4 через UART / COM-порт из Windows-программы.

Это версия без Wi-Fi и без HTTP. Управление идет напрямую через USB-TTL адаптер.

## Что входит

- `ESP32_CAM_LED_UART_Toggle/ESP32_CAM_LED_UART_Toggle.ino` — скетч для ESP32-CAM.
- `Windows_GUI/esp32cam_led_uart_gui.py` — GUI-программа под Windows на Python/Tkinter.
- `Windows_GUI/requirements.txt` — зависимость `pyserial`.
- `Windows_GUI/install_requirements.bat` — установка зависимости двойным кликом.
- `Windows_GUI/run_gui.bat` — запуск GUI двойным кликом.

## Логика работы

ESP32-CAM слушает команды в Serial/UART на скорости `115200`.
Windows GUI открывает выбранный COM-порт и отправляет команды.

Кнопка в GUI работает как залипающий переключатель:

1. нажали один раз — LED включился;
2. нажали второй раз — LED выключился;
3. следующее нажатие снова включает LED.

## UART-команды

ESP32-CAM принимает текстовые команды, каждая команда заканчивается переводом строки:

- `ON` — включить LED;
- `OFF` — выключить LED;
- `TOGGLE` — переключить LED;
- `STATUS` — запросить текущее состояние;
- `HELP` — показать список команд.

Ответы ESP32-CAM:

- `READY:ESP32-CAM_LED_UART`
- `STATE:ON`
- `STATE:OFF`
- `ERROR:UNKNOWN_COMMAND`

## Подключение UART

Типовое подключение через USB-TTL адаптер:

| USB-TTL адаптер | ESP32-CAM |
|---|---|
| GND | GND |
| TX | U0R / RX0 |
| RX | U0T / TX0 |
| 5V | 5V |

Важно: TX и RX соединяются крест-накрест: TX адаптера идет на RX ESP32-CAM, RX адаптера идет на TX ESP32-CAM.

Если плата питается нестабильно, лучше использовать надежное питание 5V. ESP32-CAM с камерой и вспышкой может потреблять заметный ток.

## Порядок запуска

### 1. Прошить ESP32-CAM

1. Открыть `ESP32_CAM_LED_UART_Toggle.ino` в Arduino IDE.
2. Выбрать плату `AI Thinker ESP32-CAM`.
3. Подключить ESP32-CAM в режим прошивки: GPIO0 соединить с GND.
4. Загрузить скетч.
5. Разомкнуть GPIO0-GND.
6. Нажать RST на самой плате ESP32-CAM.
7. Оставить USB-TTL подключенным к ПК.

### 2. Проверить через Serial Monitor

1. Открыть Arduino IDE → Serial Monitor.
2. Выбрать скорость `115200`.
3. После перезапуска ESP32-CAM должны появиться строки:

```text
READY:ESP32-CAM_LED_UART
COMMANDS: ON, OFF, TOGGLE, STATUS, HELP
STATE:OFF
```

4. Можно вручную отправить команду `TOGGLE`.

### 3. Установить зависимость GUI

В папке `Windows_GUI` запустить:

```bat
install_requirements.bat
```

Или вручную:

```bat
python -m pip install -r requirements.txt
```

### 4. Запустить Windows GUI

В папке `Windows_GUI` запустить:

```bat
run_gui.bat
```

Дальше:

1. выбрать COM-порт USB-TTL адаптера;
2. нажать `Подключить`;
3. нажимать большую кнопку LED: одно нажатие включает, следующее выключает.

## Важно

GPIO 4 на многих ESP32-CAM подключен к яркому светодиоду-вспышке. Не смотрите прямо на светодиод при включении.

## Отличие от HTTP-версии

- HTTP-версия работает через Wi-Fi и IP-адрес ESP32-CAM.
- UART-версия работает напрямую через COM-порт и не требует Wi-Fi.
