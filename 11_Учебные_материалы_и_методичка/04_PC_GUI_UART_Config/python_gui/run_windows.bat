@echo off
cd /d %~dp0
python -m pip install -r requirements.txt
python esp32_cam_uart_gui.py
pause
