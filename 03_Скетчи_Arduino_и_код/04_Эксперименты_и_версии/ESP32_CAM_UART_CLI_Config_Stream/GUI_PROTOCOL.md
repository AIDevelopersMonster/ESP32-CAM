# UART GUI protocol

This sketch is prepared for a future PC GUI application.

## Serial

```text
115200 baud
Newline line ending
```

## GUI sends

```text
SSID;PASSWORD
```

The sketch also accepts `SSID|PASSWORD` and `SSID,PASSWORD`, but the GUI should use the semicolon format.

## GUI reads

The PC program should ignore normal text and parse only lines that start with:

```text
GUI:
```

## Main events

```text
GUI:BOOT;NAME=ESP32-CAM_UART_CLI;BAUD=115200
GUI:READY;FORMAT=SSID;PASSWORD;BAUD=115200
GUI:SAVED;SSID=MyHomeWiFi
GUI:CONNECTING;SSID=MyHomeWiFi
GUI:OK;IP=192.168.1.55;URL=http://192.168.1.55/;STREAM=http://192.168.1.55/stream;MDNS=http://esp32cam.local/
GUI:SERVER;URL=http://192.168.1.55/;STREAM=http://192.168.1.55/stream
GUI:CAMERA;STATUS=OK
GUI:READY_STREAM;URL=http://192.168.1.55/;STREAM=http://192.168.1.55/stream;MDNS=http://esp32cam.local/
GUI:FAIL;STATUS=CONNECT_FAILED
GUI:ERROR;CODE=BAD_FORMAT;FORMAT=SSID;PASSWORD
GUI:CLEARED
GUI:RESETTING
```

## Minimal GUI algorithm

1. Open COM port at 115200.
2. Send `SSID;PASSWORD` plus newline.
3. Wait for `GUI:OK`.
4. Extract `URL` or `STREAM`.
5. Enable an `Open browser` button.

## Notes

The password is not printed back in machine-readable events. The GUI only needs to send it once. The ESP32-CAM stores Wi-Fi data in NVS memory through `Preferences`.
