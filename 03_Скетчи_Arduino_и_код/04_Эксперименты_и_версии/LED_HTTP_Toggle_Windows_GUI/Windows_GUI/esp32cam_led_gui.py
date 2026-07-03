"""
ESP32-CAM LED Control GUI for Windows
-------------------------------------

Назначение:
    Простая Windows GUI-программа для управления светодиодом-вспышкой ESP32-CAM.
    Кнопка работает как залипающий переключатель:
      - нажали один раз — LED включился;
      - нажали второй раз — LED выключился.

Как работает:
    ESP32-CAM должна быть прошита скетчем ESP32_CAM_LED_HTTP_Toggle.ino.
    Эта программа отправляет HTTP-запросы на ESP32-CAM:
      /api/status — узнать состояние;
      /api/toggle — переключить состояние.

Зависимости:
    Только стандартная библиотека Python.
    Tkinter обычно уже входит в Python для Windows.
"""

import json
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from urllib.error import URLError, HTTPError
from urllib.request import urlopen


REQUEST_TIMEOUT_SECONDS = 3


class Esp32CamLedGui:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ESP32-CAM LED GPIO4")
        self.root.geometry("430x260")
        self.root.minsize(430, 260)

        # None = состояние еще неизвестно, False = выключено, True = включено.
        self.led_state: bool | None = None

        self.ip_var = tk.StringVar(value="192.168.1.100")
        self.status_var = tk.StringVar(value="Введите IP ESP32-CAM и нажмите 'Проверить'.")

        self._build_ui()
        self._set_toggle_button_unknown()

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=14)
        main.pack(fill=tk.BOTH, expand=True)

        title = ttk.Label(main, text="ESP32-CAM LED GPIO4", font=("Segoe UI", 16, "bold"))
        title.pack(anchor="w")

        hint = ttk.Label(
            main,
            text="Управление встроенной вспышкой ESP32-CAM через Wi-Fi / HTTP.",
            wraplength=390,
        )
        hint.pack(anchor="w", pady=(4, 12))

        ip_frame = ttk.Frame(main)
        ip_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(ip_frame, text="IP ESP32-CAM:").pack(side=tk.LEFT)
        ip_entry = ttk.Entry(ip_frame, textvariable=self.ip_var, width=22)
        ip_entry.pack(side=tk.LEFT, padx=(8, 8), fill=tk.X, expand=True)

        check_button = ttk.Button(ip_frame, text="Проверить", command=self.check_status)
        check_button.pack(side=tk.LEFT)

        self.toggle_button = tk.Button(
            main,
            text="LED: ?",
            font=("Segoe UI", 18, "bold"),
            height=2,
            command=self.toggle_led,
        )
        self.toggle_button.pack(fill=tk.X, pady=(6, 12))

        status_label = ttk.Label(main, textvariable=self.status_var, wraplength=390)
        status_label.pack(anchor="w", fill=tk.X)

        footer = ttk.Label(
            main,
            text="Подсказка: IP смотрим в Arduino IDE → Serial Monitor, скорость 115200.",
            wraplength=390,
        )
        footer.pack(anchor="w", pady=(12, 0))

    def _base_url(self) -> str:
        raw = self.ip_var.get().strip()
        if not raw:
            raise ValueError("IP-адрес не указан.")

        # Разрешаем вводить как 192.168.1.100, так и http://192.168.1.100/.
        if not raw.startswith("http://") and not raw.startswith("https://"):
            raw = "http://" + raw

        return raw.rstrip("/")

    def _request_json(self, endpoint: str) -> dict:
        url = self._base_url() + endpoint
        with urlopen(url, timeout=REQUEST_TIMEOUT_SECONDS) as response:
            data = response.read().decode("utf-8")
        return json.loads(data)

    def _run_background(self, task, on_success) -> None:
        def worker() -> None:
            try:
                result = task()
            except ValueError as exc:
                self.root.after(0, lambda: self._show_error(str(exc)))
            except (URLError, HTTPError, TimeoutError, json.JSONDecodeError) as exc:
                self.root.after(0, lambda: self._show_error(f"Нет связи с ESP32-CAM: {exc}"))
            except Exception as exc:  # Защита GUI от неожиданного падения.
                self.root.after(0, lambda: self._show_error(f"Ошибка: {exc}"))
            else:
                self.root.after(0, lambda: on_success(result))

        threading.Thread(target=worker, daemon=True).start()

    def _show_error(self, text: str) -> None:
        self.status_var.set(text)
        self._set_toggle_button_unknown()
        messagebox.showwarning("ESP32-CAM LED", text)

    def _apply_led_state(self, data: dict) -> None:
        self.led_state = bool(data.get("led", False))
        gpio = data.get("gpio", 4)

        if self.led_state:
            self.toggle_button.config(
                text="LED ВКЛЮЧЕН — нажмите, чтобы выключить",
                relief=tk.SUNKEN,
                bg="#ffd966",
                activebackground="#f1c232",
            )
            self.status_var.set(f"LED включен. GPIO: {gpio}.")
        else:
            self.toggle_button.config(
                text="LED ВЫКЛЮЧЕН — нажмите, чтобы включить",
                relief=tk.RAISED,
                bg="SystemButtonFace",
                activebackground="SystemButtonFace",
            )
            self.status_var.set(f"LED выключен. GPIO: {gpio}.")

    def _set_toggle_button_unknown(self) -> None:
        self.led_state = None
        self.toggle_button.config(
            text="LED: состояние неизвестно",
            relief=tk.RAISED,
            bg="SystemButtonFace",
            activebackground="SystemButtonFace",
        )

    def check_status(self) -> None:
        self.status_var.set("Проверяю связь с ESP32-CAM...")
        self._run_background(lambda: self._request_json("/api/status"), self._apply_led_state)

    def toggle_led(self) -> None:
        self.status_var.set("Отправляю команду переключения LED...")
        self._run_background(lambda: self._request_json("/api/toggle"), self._apply_led_state)


def main() -> None:
    root = tk.Tk()
    app = Esp32CamLedGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
