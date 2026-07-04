# UART CLI: ввод Wi-Fi через Serial Monitor

В этом варианте данные Wi-Fi передаются в ESP32-CAM через UART одной строкой.

## Формат

```text
SSID;PASSWORD
```

После получения строки ESP32-CAM сохраняет данные во внутреннюю NVS-память, подключается к роутеру и печатает IP в Serial Monitor.

## Скетч

```text
03_Скетчи_Arduino_и_код/04_Эксперименты_и_версии/ESP32_CAM_UART_CLI_Config_Stream/ESP32_CAM_UART_CLI_Config_Stream.ino
```

## Serial Monitor

```text
Baud: 115200
Line ending: Newline
```

## Команды

```text
HELP   - подсказка
SHOW   - показать сохраненный SSID
RESET  - стереть Wi-Fi настройки
```

## После подключения

Открывай главную страницу камеры:

```text
http://IP_КАМЕРЫ/
```

Видеопоток:

```text
http://IP_КАМЕРЫ/stream
```

Этот режим удобен как основа для будущей PC GUI-программы: окно на компьютере отправит строку по COM-порту, прочитает IP и откроет браузер кнопкой.
