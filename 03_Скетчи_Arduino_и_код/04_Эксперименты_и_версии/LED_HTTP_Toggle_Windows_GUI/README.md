# LED HTTP Toggle + Windows GUI

Проект для управления встроенным светодиодом-вспышкой ESP32-CAM на GPIO 4 из Windows-программы.

## Что входит

- `ESP32_CAM_LED_HTTP_Toggle/ESP32_CAM_LED_HTTP_Toggle.ino` — скетч для ESP32-CAM.
- `Windows_GUI/esp32cam_led_gui.py` — GUI-программа под Windows на Python/Tkinter.
- `Windows_GUI/run_gui.bat` — запуск GUI двойным кликом.

## Логика работы

ESP32-CAM подключается к Wi-Fi и запускает HTTP-сервер. Windows GUI отправляет команды на IP-адрес ESP32-CAM.

Кнопка в GUI работает как залипающий переключатель:

1. нажали один раз — LED включился;
2. нажали второй раз — LED выключился;
3. следующее нажатие снова включает LED.

## HTTP-команды ESP32-CAM

После прошивки и подключения к Wi-Fi доступны адреса:

- `http://IP_ESP32_CAM/` — простая web-страница управления;
- `http://IP_ESP32_CAM/api/on` — включить LED;
- `http://IP_ESP32_CAM/api/off` — выключить LED;
- `http://IP_ESP32_CAM/api/toggle` — переключить LED;
- `http://IP_ESP32_CAM/api/status` — получить состояние LED в JSON.

## Порядок запуска

### 1. Прошить ESP32-CAM

1. Открыть `ESP32_CAM_LED_HTTP_Toggle.ino` в Arduino IDE.
2. Вписать имя и пароль Wi-Fi:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

3. Выбрать плату `AI Thinker ESP32-CAM`.
4. Подключить ESP32-CAM в режим прошивки: GPIO0 соединить с GND.
5. Загрузить скетч.
6. Разомкнуть GPIO0-GND и нажать RST на самой плате ESP32-CAM.
7. Открыть Serial Monitor на скорости `115200` и посмотреть IP-адрес.

### 2. Запустить Windows GUI

1. Открыть папку `Windows_GUI`.
2. Запустить `run_gui.bat` или выполнить команду:

```bat
python esp32cam_led_gui.py
```

3. Ввести IP-адрес ESP32-CAM.
4. Нажать `Проверить`.
5. Нажимать большую кнопку LED: одно нажатие включает, следующее выключает.

## Зависимости

Дополнительные библиотеки не нужны. Используется только стандартная библиотека Python:

- `tkinter` для окна;
- `urllib` для HTTP-запросов;
- `json` для чтения ответа ESP32-CAM.

## Важно

GPIO 4 на многих ESP32-CAM подключен к яркому светодиоду-вспышке. Не смотрите прямо на светодиод при включении.
