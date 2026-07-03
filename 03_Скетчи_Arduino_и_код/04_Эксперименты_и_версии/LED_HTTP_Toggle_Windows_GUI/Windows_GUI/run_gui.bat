@echo off
chcp 65001 >nul
title ESP32-CAM LED GUI

echo Starting ESP32-CAM LED GUI...
echo.
python esp32cam_led_gui.py

if errorlevel 1 (
    echo.
    echo Could not start GUI. Check that Python is installed and added to PATH.
    echo Download Python for Windows from https://www.python.org/downloads/windows/
    pause
)
