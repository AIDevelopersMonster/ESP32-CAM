# ESP32_CAM_Simple_CLI_Stream

Рабочий простой скетч для ESP32-CAM AI Thinker.

Цель версии: оставить минимальную и понятную схему запуска без GUI-протокола.

## Что делает скетч

- запускает камеру ESP32-CAM AI Thinker;
- читает Wi-Fi данные через Serial Monitor;
- сохраняет SSID и ключ в памяти ESP32 через `Preferences`;
- при следующем запуске пробует подключиться к сохранённой сети;
- если сохранённая сеть не найдена или подключение не удалось, снова ждёт ввод через Serial Monitor;
- после подключения печатает IP-адрес;
- запускает MJPEG-поток прямо на корневом URL `/`;
- дополнительно поднимает mDNS-имя `esp32cam.local`.

## Где находится

```text
03_Скетчи_Arduino_и_код/04_Эксперименты_и_версии/ESP32_CAM_Simple_CLI_Stream/ESP32_CAM_Simple_CLI_Stream.ino
```

## Настройки Arduino IDE

Типовые настройки для AI Thinker ESP32-CAM:

```text
Board: AI Thinker ESP32-CAM
Upload Speed: 115200 или 921600
Flash Frequency: 40MHz
Partition Scheme: Huge APP или подходящая схема для CameraWebServer
Serial Monitor: 115200
Line ending: Newline или Both NL & CR
```

Для прошивки обычно нужно замкнуть `GPIO0` на `GND`, затем после загрузки отключить `GPIO0` от `GND` и нажать `RST/EN` на самой ESP32-CAM.

## Ввод Wi-Fi через Serial Monitor

После старта скетч выводит:

```text
ESP32-CAM Simple CLI Stream
Camera initialized

=== ESP32-CAM Simple CLI Stream ===
Enter in Serial Monitor:
  SSID;PASS
Commands: HELP, SHOW, RESET
```

В Serial Monitor нужно ввести одну строку:

```text
ИМЯ_СЕТИ;КЛЮЧ_СЕТИ
```

Пример:

```text
MyHomeWiFi;MySecretKey
```

После успешного подключения скетч сохраняет данные в память ESP32.

## Команды

```text
HELP  - показать краткую справку
SHOW  - показать сохранённый SSID, маскированный ключ, статус Wi-Fi и IP
RESET - стереть сохранённые Wi-Fi данные и перезагрузить плату
```

`RESET` полезен, если ранее были сохранены неправильные данные, лишний пробел, невидимый символ или данные были записаны через старую GUI-версию.

## Успешный запуск

Нормальный успешный лог выглядит примерно так:

```text
WiFi connected
IP address: 192.168.x.x
mDNS ready: http://esp32cam.local/
Camera Stream Ready! Go to: http://192.168.x.x
mDNS URL: http://esp32cam.local/
HTTP stream server started on /
```

Открывать в браузере нужно один из адресов:

```text
http://192.168.x.x
```

или:

```text
http://esp32cam.local/
```

Важно: поток в этой версии висит прямо на `/`, а не на `/stream`.

## Если `esp32cam.local` не открывается

На Windows mDNS-имена `.local` могут не открываться без Bonjour/mDNS-службы. В этом случае использовать числовой IP-адрес из Serial Monitor.

## Замечание по качеству изображения

Эта версия специально сделана как простая рабочая база: CLI + сохранение Wi-Fi + поток.

Если изображение хуже, чем в прежнем скетче, это отдельный следующий шаг: вернуть или подобрать параметры камеры (`frame_size`, `jpeg_quality`, `fb_count`) уже после фиксации стабильного подключения.

## Вывод по проблеме GUI

Практическая проверка показала: после очистки сохранённых Wi-Fi данных через `RESET` и ручного ввода через Serial Monitor подключение заработало. Значит вероятная причина сбоя GUI-версии была в сохранённой строке SSID/ключа: лишний пробел, невидимый символ, неверная вставка или некорректная запись в `Preferences`.
