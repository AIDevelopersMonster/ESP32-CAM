"""ESP32-CAM UART Wi-Fi GUI.

This GUI talks to ESP32_CAM_UART_CLI_Config_Stream.ino over a serial port.
It sends one line: SSID;PASSWORD
It parses only machine-readable lines that start with: GUI:
"""

import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk
import webbrowser

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover
    serial = None
    list_ports = None


BAUD_RATE = 115200
READ_TIMEOUT = 0.2


class Esp32CamUartGui:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ESP32-CAM UART Wi-Fi GUI")
        self.root.geometry("760x560")
        self.root.minsize(720, 500)

        self.serial_port = None
        self.reader_thread = None
        self.reader_running = False
        self.events = queue.Queue()

        self.current_url = ""
        self.current_stream = ""
        self.current_mdns = ""

        self.port_var = tk.StringVar()
        self.ssid_var = tk.StringVar()
        self.password_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Not connected")
        self.ip_var = tk.StringVar(value="-")
        self.url_var = tk.StringVar(value="-")
        self.stream_var = tk.StringVar(value="-")
        self.show_password_var = tk.BooleanVar(value=False)

        self._build_ui()
        self.refresh_ports()
        self.root.after(100, self.process_events)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        if serial is None:
            messagebox.showerror(
                "pyserial is missing",
                "Install dependency first:\n\npython -m pip install -r requirements.txt",
            )

    def _build_ui(self) -> None:
        root = self.root
        root.columnconfigure(0, weight=1)
        root.rowconfigure(4, weight=1)

        connection = ttk.LabelFrame(root, text="1. Serial connection")
        connection.grid(row=0, column=0, padx=10, pady=8, sticky="ew")
        connection.columnconfigure(1, weight=1)

        ttk.Label(connection, text="COM port:").grid(row=0, column=0, padx=6, pady=8, sticky="w")
        self.port_combo = ttk.Combobox(connection, textvariable=self.port_var, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=6, pady=8, sticky="ew")
        ttk.Button(connection, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=6, pady=8)
        self.connect_button = ttk.Button(connection, text="Connect", command=self.connect_serial)
        self.connect_button.grid(row=0, column=3, padx=6, pady=8)
        self.disconnect_button = ttk.Button(connection, text="Disconnect", command=self.disconnect_serial, state="disabled")
        self.disconnect_button.grid(row=0, column=4, padx=6, pady=8)

        wifi = ttk.LabelFrame(root, text="2. Wi-Fi credentials")
        wifi.grid(row=1, column=0, padx=10, pady=8, sticky="ew")
        wifi.columnconfigure(1, weight=1)

        ttk.Label(wifi, text="SSID:").grid(row=0, column=0, padx=6, pady=8, sticky="w")
        ttk.Entry(wifi, textvariable=self.ssid_var).grid(row=0, column=1, padx=6, pady=8, sticky="ew")

        ttk.Label(wifi, text="Password:").grid(row=1, column=0, padx=6, pady=8, sticky="w")
        self.password_entry = ttk.Entry(wifi, textvariable=self.password_var, show="*")
        self.password_entry.grid(row=1, column=1, padx=6, pady=8, sticky="ew")
        ttk.Checkbutton(wifi, text="Show", variable=self.show_password_var, command=self.toggle_password).grid(row=1, column=2, padx=6, pady=8)

        self.send_button = ttk.Button(wifi, text="Send SSID;PASSWORD", command=self.send_credentials, state="disabled")
        self.send_button.grid(row=0, column=2, padx=6, pady=8, sticky="ew")
        self.reset_button = ttk.Button(wifi, text="RESET saved Wi-Fi", command=self.send_reset, state="disabled")
        self.reset_button.grid(row=0, column=3, rowspan=2, padx=6, pady=8, sticky="ns")

        result = ttk.LabelFrame(root, text="3. ESP32-CAM result")
        result.grid(row=2, column=0, padx=10, pady=8, sticky="ew")
        result.columnconfigure(1, weight=1)

        ttk.Label(result, text="Status:").grid(row=0, column=0, padx=6, pady=4, sticky="w")
        ttk.Label(result, textvariable=self.status_var).grid(row=0, column=1, padx=6, pady=4, sticky="w")
        ttk.Label(result, text="IP:").grid(row=1, column=0, padx=6, pady=4, sticky="w")
        ttk.Label(result, textvariable=self.ip_var).grid(row=1, column=1, padx=6, pady=4, sticky="w")
        ttk.Label(result, text="Page:").grid(row=2, column=0, padx=6, pady=4, sticky="w")
        ttk.Label(result, textvariable=self.url_var).grid(row=2, column=1, padx=6, pady=4, sticky="w")
        ttk.Label(result, text="Stream:").grid(row=3, column=0, padx=6, pady=4, sticky="w")
        ttk.Label(result, textvariable=self.stream_var).grid(row=3, column=1, padx=6, pady=4, sticky="w")

        buttons = ttk.Frame(root)
        buttons.grid(row=3, column=0, padx=10, pady=6, sticky="ew")
        buttons.columnconfigure(4, weight=1)
        self.open_page_button = ttk.Button(buttons, text="Open camera page", command=self.open_page, state="disabled")
        self.open_page_button.grid(row=0, column=0, padx=4, pady=4)
        self.open_stream_button = ttk.Button(buttons, text="Open video stream", command=self.open_stream, state="disabled")
        self.open_stream_button.grid(row=0, column=1, padx=4, pady=4)
        ttk.Button(buttons, text="SHOW", command=self.send_show).grid(row=0, column=2, padx=4, pady=4)
        ttk.Button(buttons, text="HELP", command=self.send_help).grid(row=0, column=3, padx=4, pady=4)
        ttk.Button(buttons, text="Clear log", command=self.clear_log).grid(row=0, column=5, padx=4, pady=4)

        log_frame = ttk.LabelFrame(root, text="Serial log")
        log_frame.grid(row=4, column=0, padx=10, pady=8, sticky="nsew")
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=12, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scrollbar.set)

    def refresh_ports(self) -> None:
        if list_ports is None:
            self.port_combo["values"] = []
            return
        ports = [port.device for port in list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
        self.log(f"Ports: {', '.join(ports) if ports else 'none'}")

    def connect_serial(self) -> None:
        if serial is None:
            messagebox.showerror("Error", "pyserial is not installed")
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("No port", "Select a COM port first")
            return
        try:
            self.serial_port = serial.Serial(port, BAUD_RATE, timeout=READ_TIMEOUT)
            time.sleep(1.5)
            self.reader_running = True
            self.reader_thread = threading.Thread(target=self.reader_loop, daemon=True)
            self.reader_thread.start()
            self.status_var.set(f"Connected to {port}")
            self.connect_button.configure(state="disabled")
            self.disconnect_button.configure(state="normal")
            self.send_button.configure(state="normal")
            self.reset_button.configure(state="normal")
            self.log(f"Connected to {port} at {BAUD_RATE}")
        except Exception as exc:
            messagebox.showerror("Connection failed", str(exc))

    def disconnect_serial(self) -> None:
        self.reader_running = False
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self.connect_button.configure(state="normal")
        self.disconnect_button.configure(state="disabled")
        self.send_button.configure(state="disabled")
        self.reset_button.configure(state="disabled")
        self.status_var.set("Disconnected")
        self.log("Disconnected")

    def reader_loop(self) -> None:
        while self.reader_running and self.serial_port is not None:
            try:
                raw = self.serial_port.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").strip()
                    if line:
                        self.events.put(line)
            except Exception as exc:
                self.events.put(f"GUI:ERROR;CODE=SERIAL_READ_FAILED;DETAIL={exc}")
                break

    def process_events(self) -> None:
        try:
            while True:
                line = self.events.get_nowait()
                self.log(line)
                if line.startswith("GUI:"):
                    self.handle_gui_event(line[4:])
        except queue.Empty:
            pass
        self.root.after(100, self.process_events)

    def handle_gui_event(self, payload: str) -> None:
        parts = payload.split(";")
        event = parts[0]
        data = {}
        for item in parts[1:]:
            if "=" in item:
                key, value = item.split("=", 1)
                data[key] = value

        if event == "BOOT":
            self.status_var.set("ESP32-CAM booted")
        elif event == "READY":
            self.status_var.set("Ready for SSID;PASSWORD")
        elif event == "CONNECTING":
            self.status_var.set(f"Connecting to {data.get('SSID', '')}")
        elif event == "SAVED":
            self.status_var.set("Credentials saved")
        elif event == "OK":
            self.status_var.set("Wi-Fi connected")
            self.apply_urls(data)
        elif event == "READY_STREAM":
            self.status_var.set("Camera stream ready")
            self.apply_urls(data)
        elif event == "SERVER":
            self.apply_urls(data)
        elif event == "CAMERA":
            self.status_var.set("Camera initialized")
        elif event == "FAIL":
            self.status_var.set(f"Wi-Fi failed: {data.get('STATUS', 'unknown')}")
        elif event == "ERROR":
            self.status_var.set(f"Error: {data.get('CODE', payload)}")
        elif event == "CLEARED":
            self.status_var.set("Saved Wi-Fi cleared")

    def apply_urls(self, data: dict) -> None:
        ip = data.get("IP")
        url = data.get("URL")
        stream = data.get("STREAM")
        mdns = data.get("MDNS")

        if ip:
            self.ip_var.set(ip)
        if url:
            self.current_url = url
            self.url_var.set(url)
            self.open_page_button.configure(state="normal")
        if stream:
            self.current_stream = stream
            self.stream_var.set(stream)
            self.open_stream_button.configure(state="normal")
        if mdns:
            self.current_mdns = mdns

    def send_line(self, line: str) -> None:
        if self.serial_port is None or not self.serial_port.is_open:
            messagebox.showwarning("Not connected", "Connect to COM port first")
            return
        try:
            self.serial_port.write((line + "\n").encode("utf-8"))
            self.serial_port.flush()
            safe_line = line
            if ";" in line and not line.upper().startswith(("HELP", "SHOW", "RESET")):
                ssid = line.split(";", 1)[0]
                safe_line = ssid + ";********"
            self.log("> " + safe_line)
        except Exception as exc:
            messagebox.showerror("Send failed", str(exc))

    def send_credentials(self) -> None:
        ssid = self.ssid_var.get().strip()
        password = self.password_var.get()
        if not ssid:
            messagebox.showwarning("SSID is empty", "Enter Wi-Fi SSID")
            return
        self.status_var.set("Sending credentials")
        self.send_line(f"{ssid};{password}")

    def send_reset(self) -> None:
        if messagebox.askyesno("Reset Wi-Fi", "Erase saved Wi-Fi settings on ESP32-CAM?"):
            self.send_line("RESET")

    def send_show(self) -> None:
        self.send_line("SHOW")

    def send_help(self) -> None:
        self.send_line("HELP")

    def open_page(self) -> None:
        if self.current_url:
            webbrowser.open(self.current_url)

    def open_stream(self) -> None:
        if self.current_stream:
            webbrowser.open(self.current_stream)

    def toggle_password(self) -> None:
        self.password_entry.configure(show="" if self.show_password_var.get() else "*")

    def clear_log(self) -> None:
        self.log_text.delete("1.0", "end")

    def log(self, message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {message}\n")
        self.log_text.see("end")

    def on_close(self) -> None:
        self.disconnect_serial()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    app = Esp32CamUartGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
