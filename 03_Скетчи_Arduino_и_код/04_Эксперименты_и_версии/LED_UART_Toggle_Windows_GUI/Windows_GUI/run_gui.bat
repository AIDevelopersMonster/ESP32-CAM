@echo off
chcp 65001 >nul
title ESP32-CAM LED UART GUI

echo Starting ESP32-CAM LED UART GUI...
echo.
python esp32cam_led_uart_gui.py

if errorlevel 1 (
    echo.
    echo Could not start GUI.
    echo Check that Python is installed and added to PATH.
    echo Then run install_requirements.bat to install pyserial.
    pause
)
