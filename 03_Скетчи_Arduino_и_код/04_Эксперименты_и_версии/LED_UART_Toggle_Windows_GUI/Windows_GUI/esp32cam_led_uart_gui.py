"""
ESP32-CAM LED UART Control GUI for Windows
------------------------------------------

Назначение:
    Windows GUI-программа для управления светодиодом-вспышкой ESP32-CAM
    через UART / COM-порт.

Логика кнопки:
    - нажали один раз — LED включился;
    - нажали второй раз — LED выключился.

Как работает:
    1. ESP32-CAM прошивается скетчем ESP32_CAM_LED_UART_Toggle.ino.
    2. ПК подключается к ESP32-CAM через USB-TTL адаптер.
    3. Эта программа открывает COM-порт и отправляет команды:
       STATUS, TOGGLE, ON, OFF.

Зависимость:
    pip install pyserial
"""

import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Optional

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


BAUD_RATE = 115200
READ_TIMEOUT_SECONDS = 0.2
COMMAND_TIMEOUT_SECONDS = 3.0


class Esp32CamLedUartGui:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ESP32-CAM LED UART GPIO4")
        self.root.geometry("520x340")
        self.root.minsize(520, 340)

        self.serial_port = None
        self.reader_thread = None
        self.reader_running = False
        self.response_queue: queue.Queue[str] = queue.Queue()
        self.led_state: Optional[bool] = None

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Выберите COM-порт и нажмите 'Подключить'.")
        self.log_var = tk.StringVar(value="")

        self._build_ui()
        self.refresh_ports()
        self._set_disconnected_ui()

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=14)
        main.pack(fill=tk.BOTH, expand=True)

        title = ttk.Label(main, text="ESP32-CAM LED UART GPIO4", font=("Segoe UI", 16, "bold"))
        title.pack(anchor="w")

        hint = ttk.Label(
            main,
            text="Управление встроенной вспышкой ESP32-CAM через USB-TTL / COM-порт.",
            wraplength=480,
        )
        hint.pack(anchor="w", pady=(4, 12))

        port_frame = ttk.Frame(main)
        port_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(port_frame, text="COM-порт:").pack(side=tk.LEFT)

        self.port_combo = ttk.Combobox(port_frame, textvariable=self.port_var, width=24, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=(8, 8), fill=tk.X, expand=True)

        self.refresh_button = ttk.Button(port_frame, text="Обновить", command=self.refresh_ports)
        self.refresh_button.pack(side=tk.LEFT, padx=(0, 6))

        self.connect_button = ttk.Button(port_frame, text="Подключить", command=self.toggle_connection)
        self.connect_button.pack(side=tk.LEFT)

        self.toggle_button = tk.Button(
            main,
            text="LED: нет подключения",
            font=("Segoe UI", 17, "bold"),
            height=2,
            command=self.toggle_led,
        )
        self.toggle_button.pack(fill=tk.X, pady=(6, 10))

        small_buttons = ttk.Frame(main)
        small_buttons.pack(fill=tk.X, pady=(0, 10))

        self.on_button = ttk.Button(small_buttons, text="ON", command=lambda: self.send_led_command("ON"))
        self.on_button.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 4))

        self.off_button = ttk.Button(small_buttons, text="OFF", command=lambda: self.send_led_command("OFF"))
        self.off_button.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

        self.status_button = ttk.Button(small_buttons, text="STATUS", command=lambda: self.send_led_command("STATUS"))
        self.status_button.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(4, 0))

        status_label = ttk.Label(main, textvariable=self.status_var, wraplength=480)
        status_label.pack(anchor="w", fill=tk.X)

        log_label = ttk.Label(main, textvariable=self.log_var, wraplength=480)
        log_label.pack(anchor="w", fill=tk.X, pady=(8, 0))

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def refresh_ports(self) -> None:
        if serial is None or list_ports is None:
            self.port_combo["values"] = []
            self.status_var.set("Не установлен pyserial. Запустите install_requirements.bat или: pip install pyserial")
            return

        ports = [port.device for port in list_ports.comports()]
        self.port_combo["values"] = ports

        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

        if not ports:
            self.status_var.set("COM-порты не найдены. Проверьте USB-TTL адаптер и драйвер CH340/CP210x.")

    def toggle_connection(self) -> None:
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        if serial is None:
            messagebox.showerror("ESP32-CAM UART", "Не установлен pyserial. Запустите install_requirements.bat")
            return

        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("ESP32-CAM UART", "Выберите COM-порт.")
            return

        try:
            self.serial_port = serial.Serial(port=port, baudrate=BAUD_RATE, timeout=READ_TIMEOUT_SECONDS)
            time.sleep(1.0)
            self.serial_port.reset_input_buffer()
            self.serial_port.reset_output_buffer()
        except Exception as exc:
            self.serial_port = None
            self.status_var.set(f"Не удалось открыть {port}: {exc}")
            messagebox.showerror("ESP32-CAM UART", f"Не удалось открыть {port}: {exc}")
            return

        self.reader_running = True
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

        self._set_connected_ui()
        self.status_var.set(f"Подключено к {port} на скорости {BAUD_RATE}. Запрашиваю состояние LED...")
        self.send_led_command("STATUS")

    def disconnect(self) -> None:
        self.reader_running = False

        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass

        self.serial_port = None
        self._set_disconnected_ui()
        self.status_var.set("Отключено.")

    def _reader_loop(self) -> None:
        while self.reader_running and self.serial_port and self.serial_port.is_open:
            try:
                raw_line = self.serial_port.readline()
            except Exception as exc:
                self.root.after(0, lambda err=str(exc): self._handle_serial_error(err))
                break

            if not raw_line:
                continue

            try:
                line = raw_line.decode("utf-8", errors="replace").strip()
            except Exception:
                line = str(raw_line).strip()

            if line:
                self.response_queue.put(line)
                self.root.after(0, lambda text=line: self._append_log(text))
                self.root.after(0, lambda text=line: self._process_line(text))

    def _handle_serial_error(self, text: str) -> None:
        self.status_var.set(f"Ошибка COM-порта: {text}")
        self.disconnect()

    def _append_log(self, line: str) -> None:
        self.log_var.set(f"Последний ответ ESP32-CAM: {line}")

    def _process_line(self, line: str) -> None:
        if line == "STATE:ON":
            self._apply_led_state(True)
        elif line == "STATE:OFF":
            self._apply_led_state(False)
        elif line.startswith("ERROR:"):
            self.status_var.set(f"ESP32-CAM вернула ошибку: {line}")

    def _write_command(self, command: str) -> None:
        if not self.serial_port or not self.serial_port.is_open:
            raise RuntimeError("COM-порт не открыт.")

        payload = (command.strip().upper() + "\n").encode("utf-8")
        self.serial_port.write(payload)
        self.serial_port.flush()

    def send_led_command(self, command: str) -> None:
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showwarning("ESP32-CAM UART", "Сначала подключитесь к COM-порту.")
            return

        try:
            self._write_command(command)
            self.status_var.set(f"Команда отправлена: {command.upper()}")
        except Exception as exc:
            self.status_var.set(f"Ошибка отправки команды: {exc}")
            messagebox.showerror("ESP32-CAM UART", f"Ошибка отправки команды: {exc}")

    def toggle_led(self) -> None:
        self.send_led_command("TOGGLE")

    def _apply_led_state(self, state: bool) -> None:
        self.led_state = state

        if state:
            self.toggle_button.config(
                text="LED ВКЛЮЧЕН — нажмите, чтобы выключить",
                relief=tk.SUNKEN,
                bg="#ffd966",
                activebackground="#f1c232",
            )
            self.status_var.set("LED включен.")
        else:
            self.toggle_button.config(
                text="LED ВЫКЛЮЧЕН — нажмите, чтобы включить",
                relief=tk.RAISED,
                bg="SystemButtonFace",
                activebackground="SystemButtonFace",
            )
            self.status_var.set("LED выключен.")

    def _set_connected_ui(self) -> None:
        self.connect_button.config(text="Отключить")
        self.toggle_button.config(state=tk.NORMAL)
        self.on_button.config(state=tk.NORMAL)
        self.off_button.config(state=tk.NORMAL)
        self.status_button.config(state=tk.NORMAL)

    def _set_disconnected_ui(self) -> None:
        self.led_state = None
        self.connect_button.config(text="Подключить")
        self.toggle_button.config(
            text="LED: нет подключения",
            state=tk.NORMAL,
            relief=tk.RAISED,
            bg="SystemButtonFace",
            activebackground="SystemButtonFace",
        )
        self.on_button.config(state=tk.DISABLED)
        self.off_button.config(state=tk.DISABLED)
        self.status_button.config(state=tk.DISABLED)

    def on_close(self) -> None:
        self.disconnect()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    Esp32CamLedUartGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
