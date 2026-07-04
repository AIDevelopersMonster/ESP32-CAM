# ESP32-CAM UART Wi-Fi GUI

Простая программа на Python для настройки Wi-Fi у ESP32-CAM через COM-порт.

## Связанная прошивка

```text
03_Скетчи_Arduino_и_код/04_Эксперименты_и_версии/ESP32_CAM_UART_CLI_Config_Stream/ESP32_CAM_UART_CLI_Config_Stream.ino
```

## Возможности

- выбор COM-порта;
- подключение на скорости `115200`;
- отправка строки `SSID;PASSWORD`;
- чтение событий `GUI:` от ESP32-CAM;
- отображение IP, URL страницы и URL видеопотока;
- кнопки открытия страницы камеры и видеопотока в браузере.

## Установка

Нужен Python 3.

```text
python -m pip install -r requirements.txt
```

## Запуск

```text
python esp32_cam_uart_gui.py
```

## Порядок работы

1. Прошить ESP32-CAM UART CLI скетчем.
2. Отключить `GPIO0` от `GND` после прошивки.
3. Перезагрузить ESP32-CAM.
4. Запустить GUI.
5. Нажать `Refresh`.
6. Выбрать COM-порт.
7. Нажать `Connect`.
8. Ввести SSID и пароль.
9. Нажать `Send SSID;PASSWORD`.
10. После получения IP нажать `Open camera page` или `Open video stream`.

## Протокол

GUI читает только строки с префиксом:

```text
GUI:
```

Успешное подключение выглядит так:

```text
GUI:OK;IP=192.168.1.55;URL=http://192.168.1.55/;STREAM=http://192.168.1.55/stream;MDNS=http://esp32cam.local/
```

## Кнопки

- `Send SSID;PASSWORD` — отправить данные Wi-Fi.
- `RESET saved Wi-Fi` — стереть сохраненный Wi-Fi на ESP32-CAM.
- `SHOW` — запросить сохраненный SSID.
- `HELP` — запросить подсказку прошивки.
- `Open camera page` — открыть главную страницу камеры.
- `Open video stream` — открыть MJPEG поток.

## Важно

Пароль не выводится в лог GUI в открытом виде.
