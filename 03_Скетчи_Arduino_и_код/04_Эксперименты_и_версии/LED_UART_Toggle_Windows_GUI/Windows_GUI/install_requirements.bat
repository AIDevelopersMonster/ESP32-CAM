@echo off
chcp 65001 >nul
title Install ESP32-CAM UART GUI requirements

echo Installing Python requirements for ESP32-CAM UART GUI...
echo.
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

echo.
echo Done.
pause
