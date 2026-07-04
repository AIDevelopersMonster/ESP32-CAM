"""Simple ESP32-CAM UART GUI.

Use with ESP32_CAM_UART_CLI_Config_Stream.ino.
Send one line to ESP32-CAM: SSID;PASSWORD
Parse only lines that start with GUI:
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
except ImportError:
    serial = None
    list_ports = None

BAUD = 115200


class App:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32-CAM UART GUI Simple")
        self.root.geometry("760x560")
        self.ser = None
        self.reader_on = False
        self.q = queue.Queue()
        self.url = ""
        self.stream = ""

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Not connected")
        self.ip_var = tk.StringVar(value="-")
        self.url_var = tk.StringVar(value="-")
        self.stream_var = tk.StringVar(value="-")

        self.build()
        self.refresh_ports()
        self.root.after(100, self.process_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        if serial is None:
            messagebox.showerror("Missing pyserial", "Run: python -m pip install -r requirements.txt")

    def build(self):
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(4, weight=1)

        top = ttk.LabelFrame(self.root, text="1. COM port")
        top.grid(row=0, column=0, padx=10, pady=8, sticky="ew")
        top.columnconfigure(1, weight=1)
        ttk.Label(top, text="Port:").grid(row=0, column=0, padx=6, pady=6)
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=6, pady=6, sticky="ew")
        ttk.Button(top, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=6)
        self.connect_btn = ttk.Button(top, text="Connect", command=self.connect)
        self.connect_btn.grid(row=0, column=3, padx=6)
        self.disconnect_btn = ttk.Button(top, text="Disconnect", command=self.disconnect, state="disabled")
        self.disconnect_btn.grid(row=0, column=4, padx=6)

        cmd = ttk.LabelFrame(self.root, text="2. Paste or type full command")
        cmd.grid(row=1, column=0, padx=10, pady=8, sticky="ew")
        cmd.columnconfigure(0, weight=1)
        ttk.Label(cmd, text="Format: SSID;PASSWORD").grid(row=0, column=0, columnspan=3, padx=6, pady=2, sticky="w")
        self.command_text = tk.Text(cmd, height=3, undo=True)
        self.command_text.grid(row=1, column=0, padx=6, pady=6, sticky="ew")
        self.command_text.bind("<Control-a>", self.select_all_command)
        self.command_text.bind("<Control-A>", self.select_all_command)
        self.command_text.bind("<Button-3>", self.popup_menu)
        self.menu = tk.Menu(self.root, tearoff=0)
        self.menu.add_command(label="Paste", command=self.paste_command)
        self.menu.add_command(label="Select all", command=lambda: self.select_all_command(None))
        ttk.Button(cmd, text="Paste from clipboard", command=self.paste_command).grid(row=1, column=1, padx=6, pady=6, sticky="ew")
        self.send_btn = ttk.Button(cmd, text="Send to ESP32-CAM", command=self.send_command, state="disabled")
        self.send_btn.grid(row=1, column=2, padx=6, pady=6, sticky="ew")

        res = ttk.LabelFrame(self.root, text="3. Result")
        res.grid(row=2, column=0, padx=10, pady=8, sticky="ew")
        res.columnconfigure(1, weight=1)
        ttk.Label(res, text="Status:").grid(row=0, column=0, padx=6, sticky="w")
        ttk.Label(res, textvariable=self.status_var).grid(row=0, column=1, padx=6, sticky="w")
        ttk.Label(res, text="IP:").grid(row=1, column=0, padx=6, sticky="w")
        ttk.Label(res, textvariable=self.ip_var).grid(row=1, column=1, padx=6, sticky="w")
        ttk.Label(res, text="Page:").grid(row=2, column=0, padx=6, sticky="w")
        ttk.Label(res, textvariable=self.url_var).grid(row=2, column=1, padx=6, sticky="w")
        ttk.Label(res, text="Stream:").grid(row=3, column=0, padx=6, sticky="w")
        ttk.Label(res, textvariable=self.stream_var).grid(row=3, column=1, padx=6, sticky="w")

        buttons = ttk.Frame(self.root)
        buttons.grid(row=3, column=0, padx=10, pady=4, sticky="ew")
        self.open_page_btn = ttk.Button(buttons, text="Open camera page", command=lambda: webbrowser.open(self.url), state="disabled")
        self.open_page_btn.grid(row=0, column=0, padx=4)
        self.open_stream_btn = ttk.Button(buttons, text="Open stream", command=lambda: webbrowser.open(self.stream), state="disabled")
        self.open_stream_btn.grid(row=0, column=1, padx=4)
        ttk.Button(buttons, text="HELP", command=lambda: self.write_line("HELP")).grid(row=0, column=2, padx=4)
        ttk.Button(buttons, text="SHOW", command=lambda: self.write_line("SHOW")).grid(row=0, column=3, padx=4)
        ttk.Button(buttons, text="RESET", command=self.reset_wifi).grid(row=0, column=4, padx=4)
        ttk.Button(buttons, text="Clear log", command=lambda: self.log.delete("1.0", "end")).grid(row=0, column=5, padx=4)

        logbox = ttk.LabelFrame(self.root, text="Serial log")
        logbox.grid(row=4, column=0, padx=10, pady=8, sticky="nsew")
        logbox.columnconfigure(0, weight=1)
        logbox.rowconfigure(0, weight=1)
        self.log = tk.Text(logbox, wrap="word")
        self.log.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(logbox, command=self.log.yview)
        sb.grid(row=0, column=1, sticky="ns")
        self.log.configure(yscrollcommand=sb.set)

    def refresh_ports(self):
        if list_ports is None:
            return
        ports = [p.device for p in list_ports.comports()]
        self.port_combo["values"] = ports
        if ports:
            self.port_var.set(ports[0])
        self.add_log("Ports: " + (", ".join(ports) if ports else "none"))

    def connect(self):
        if serial is None:
            messagebox.showerror("Missing pyserial", "Install pyserial first")
            return
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("No port", "Select COM port")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.2)
            time.sleep(1.0)
            self.reader_on = True
            threading.Thread(target=self.reader, daemon=True).start()
            self.status_var.set("Connected to " + port)
            self.connect_btn.configure(state="disabled")
            self.disconnect_btn.configure(state="normal")
            self.send_btn.configure(state="normal")
            self.add_log("Connected to " + port)
        except Exception as e:
            messagebox.showerror("Connect failed", str(e))

    def disconnect(self):
        self.reader_on = False
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self.connect_btn.configure(state="normal")
        self.disconnect_btn.configure(state="disabled")
        self.send_btn.configure(state="disabled")
        self.status_var.set("Disconnected")

    def reader(self):
        while self.reader_on and self.ser:
            try:
                data = self.ser.readline()
                if data:
                    self.q.put(data.decode("utf-8", errors="replace").strip())
            except Exception as e:
                self.q.put("GUI:ERROR;CODE=SERIAL_READ;DETAIL=" + str(e))
                break

    def process_queue(self):
        try:
            while True:
                line = self.q.get_nowait()
                if line:
                    self.add_log(line)
                    if line.startswith("GUI:"):
                        self.handle_gui(line[4:])
        except queue.Empty:
            pass
        self.root.after(100, self.process_queue)

    def handle_gui(self, payload):
        parts = payload.split(";")
        event = parts[0]
        data = {}
        for part in parts[1:]:
            if "=" in part:
                k, v = part.split("=", 1)
                data[k] = v
        if event == "READY":
            self.status_var.set("ESP32-CAM ready for SSID;PASSWORD")
        elif event == "CONNECTING":
            self.status_var.set("Connecting to " + data.get("SSID", ""))
        elif event == "OK":
            self.status_var.set("Wi-Fi connected")
            self.apply_urls(data)
        elif event == "READY_STREAM":
            self.status_var.set("Stream ready")
            self.apply_urls(data)
        elif event == "SERVER":
            self.apply_urls(data)
        elif event == "FAIL":
            self.status_var.set("Wi-Fi failed: " + data.get("STATUS", "unknown"))
        elif event == "ERROR":
            self.status_var.set("Error: " + data.get("CODE", payload))

    def apply_urls(self, data):
        if data.get("IP"):
            self.ip_var.set(data["IP"])
        if data.get("URL"):
            self.url = data["URL"]
            self.url_var.set(self.url)
            self.open_page_btn.configure(state="normal")
        if data.get("STREAM"):
            self.stream = data["STREAM"]
            self.stream_var.set(self.stream)
            self.open_stream_btn.configure(state="normal")

    def command_line(self):
        return self.command_text.get("1.0", "end-1c").replace("\r", "").replace("\n", "").strip()

    def paste_command(self):
        try:
            text = self.root.clipboard_get()
        except tk.TclError:
            self.status_var.set("Clipboard is empty")
            return
        self.command_text.delete("1.0", "end")
        self.command_text.insert("1.0", str(text).replace("\r", "").replace("\n", "").strip())
        self.command_text.focus_set()

    def select_all_command(self, event):
        self.command_text.tag_add("sel", "1.0", "end-1c")
        self.command_text.mark_set("insert", "end-1c")
        return "break"

    def popup_menu(self, event):
        self.menu.tk_popup(event.x_root, event.y_root)
        return "break"

    def send_command(self):
        line = self.command_line()
        if ";" not in line:
            messagebox.showwarning("Bad format", "Use: SSID;PASSWORD")
            return
        self.write_line(line)

    def write_line(self, line):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Not connected", "Connect COM port first")
            return
        self.ser.write((line + "\n").encode("utf-8"))
        self.ser.flush()
        shown = line
        if ";" in line:
            shown = line.split(";", 1)[0] + ";********"
        self.add_log("> " + shown)

    def reset_wifi(self):
        if messagebox.askyesno("RESET", "Erase saved Wi-Fi settings on ESP32-CAM?"):
            self.write_line("RESET")

    def add_log(self, text):
        self.log.insert("end", time.strftime("[%H:%M:%S] ") + text + "\n")
        self.log.see("end")

    def close(self):
        self.disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
