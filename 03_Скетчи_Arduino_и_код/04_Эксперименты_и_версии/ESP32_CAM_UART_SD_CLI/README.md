# ESP32_CAM_UART_SD_CLI

Экспериментальный скетч для ESP32-CAM AI Thinker: работа с microSD-картой через UART CLI.

Главная идея: сначала сделать устойчивый текстовый протокол через Serial Monitor, а потом поверх этого же UART-протокола можно сделать Python/Tkinter GUI.

## Где находится

```text
03_Скетчи_Arduino_и_код/04_Эксперименты_и_версии/ESP32_CAM_UART_SD_CLI/ESP32_CAM_UART_SD_CLI.ino
```

## Настройки

```text
Board: AI Thinker ESP32-CAM
Serial Monitor: 115200
Line ending: Newline или Both NL & CR
SD mode: SD_MMC 1-bit
```

1-bit mode выбран для ESP32-CAM AI Thinker, чтобы уменьшить риск конфликтов с линиями microSD на этой плате.

## Flash LED

На ESP32-CAM AI Thinker яркий светодиод вспышки подключён к GPIO4. В этой версии он не нужен, поэтому скетч принудительно держит GPIO4 в состоянии LOW при старте, после инициализации камеры, после монтирования SD и в основном цикле.

Это важно именно для SD-эксперимента: при работе с линиями SD/камеры GPIO4 может оказаться в нежелательном состоянии и включить светодиод на максимальной яркости.

## Основные команды

```text
HELP
STATUS
MOUNT
INFO
LS;/
READ;/file.txt
WRITE;/file.txt;text
APPEND;/file.txt;text
MKDIR;/dir
CAPTURE;/photo.jpg
```

Формат команд:

```text
COMMAND;PATH;DATA
```

Для большинства команд поле `DATA` не требуется.

## Машинные строки для будущего GUI

Все строки для парсинга GUI начинаются с:

```text
SDCLI:
```

Примеры событий:

```text
SDCLI:BOOT;NAME=ESP32-CAM_UART_SD_CLI;BAUD=115200
SDCLI:READY;FORMAT=CMD;PATH;DATA;COMMANDS=HELP,STATUS,MOUNT,INFO,LS,READ,WRITE,APPEND,MKDIR,RM,RMDIR,CAPTURE
SDCLI:STATUS;SD=READY;CAMERA=READY
SDCLI:INFO;CARD=SDHC;SIZE_MB=...
SDCLI:LS_BEGIN;PATH=/
SDCLI:ITEM;TYPE=FILE;PATH=/photo.jpg;SIZE=12345
SDCLI:LS_END;PATH=/
SDCLI:OK;CMD=CAPTURE;PATH=/photo.jpg;BYTES=12345
SDCLI:ERROR;CMD=READ;CODE=OPEN_FAILED;PATH=/missing.txt
```

GUI позже должен отправлять те же команды в UART и отдельно разбирать только строки `SDCLI:`.

## Быстрая проверка

После прошивки открой Serial Monitor на `115200` и проверь:

```text
STATUS
INFO
LS;/
CAPTURE;/test.jpg
LS;/
```

Если снимок сохранён, на карте появится файл `/test.jpg`.

## Замечание

Это отдельный экспериментальный скетч. Он не заменяет рабочий `ESP32_CAM_Simple_CLI_Stream`, который уже перенесён в `main` как простая база для Wi-Fi и видеопотока.
