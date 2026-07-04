# ESP32_CAM_UART_CLI_Config_Stream

Учебный вариант ESP32-CAM, где Wi-Fi задается не через WiFiManager, а через UART/Serial CLI.

## Главная идея

В Serial Monitor отправляется одна строка:

```text
SSID;PASSWORD
```

ESP32-CAM:

1. принимает строку по UART;
2. разделяет ее по `;`;
3. сохраняет SSID и пароль во внутреннюю NVS-память через `Preferences`;
4. подключается к Wi-Fi;
5. печатает IP в Serial Monitor;
6. запускает веб-страницу камеры `/` и поток `/stream`.

## Формат команды

Основной формат:

```text
ИмяСети;Пароль
```

Пример:

```text
MyHomeWiFi;MySecretPassword
```

Также скетч принимает разделители:

```text
SSID|PASSWORD
SSID,PASSWORD
```

Но основной учебный формат — именно:

```text
SSID;PASSWORD
```

## Команды CLI

```text
HELP   - показать подсказку
SHOW   - показать сохраненный SSID и замаскированный пароль
RESET  - стереть сохраненный Wi-Fi и перезагрузить ESP32-CAM
```

## Настройки Serial Monitor

```text
Baud: 115200
Line ending: Newline или Both NL & CR
```

## Первый запуск

1. Прошить скетч.
2. Отключить `GPIO0` от `GND` после прошивки.
3. Перезагрузить ESP32-CAM.
4. Открыть Serial Monitor на `115200`.
5. Отправить строку:

```text
SSID;PASSWORD
```

6. Дождаться сообщения:

```text
WiFi connected
IP address: 192.168.1.55
Camera page: http://192.168.1.55/
Camera stream: http://192.168.1.55/stream
```

## Адреса после подключения

Главная страница:

```text
http://IP_КАМЕРЫ/
```

Видеопоток:

```text
http://IP_КАМЕРЫ/stream
```

mDNS-вариант:

```text
http://esp32cam.local/
```

Если `esp32cam.local` не работает, используй обычный IP из Serial Monitor.

## Отличие от WiFiManager-варианта

WiFiManager-вариант удобен для новичка: ESP32-CAM сама поднимает точку доступа и веб-форму.

UART CLI-вариант ближе к инженерному сервисному режиму:

```text
ПК / Serial Monitor / будущая GUI-программа -> UART -> ESP32-CAM -> NVS memory -> Wi-Fi -> IP
```

Позже на этот вариант можно поставить простую Python/C# GUI-программу, которая будет отправлять `SSID;PASSWORD` по COM-порту и открывать браузер по полученному IP.
