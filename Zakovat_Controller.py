import serial
import serial.tools.list_ports
import threading
import time
import os
from datetime import datetime
import sys
from pathlib import Path
import msvcrt

# ==== CONFIG ====
BAUD_RATE = 9600
BASE_DIR = Path(__file__).resolve().parent
LOG_DIR = BASE_DIR / 'logs'
os.makedirs(LOG_DIR, exist_ok=True)
LOG_PATH = os.path.join(LOG_DIR, f'serial_log_{datetime.now().strftime("%Y-%m-%d")}.txt')

# ==== AUTO DETECT ARDUINO COM ====
def find_arduino_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "Arduino" in p.description or "CH340" in p.description or "USB-SERIAL" in p.description:
            return p.device  # e.g., 'COM5'
    return None

SERIAL_PORT = find_arduino_port()
if SERIAL_PORT is None:
    print("⚠️  Arduino not found. Make sure it's connected.")
    sys.exit(1)

# ==== SERIAL SETUP ====
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
except Exception as e:
    print(f"[ERROR] Failed to open {SERIAL_PORT}: {e}")
    sys.exit(1)

# ==== LOGGING ====
def log_line(prefix, msg):
    timestamp = datetime.now().strftime("[%Y-%m-%d %H:%M:%S]")
    with open(LOG_PATH, 'a') as f:
        f.write(f"{timestamp} {prefix} {msg}\n")

# ==== SERIAL READER THREAD ====
def read_serial():
    while True:
        try:
            if ser.in_waiting:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    print(f"<< {line}")
                    log_line("<<", line)
        except Exception as e:
            print(f"[ERROR reading serial]: {e}")
            break

# ==== GET KEY FUNCTION ====
def get_char(timeout=0.1):
    if msvcrt.kbhit():
        return msvcrt.getwch()
    return None

# ==== SENDER ====
def send_message(msg):
    if ser and ser.is_open:
        ser.write((msg + '\n').encode())
        print(f">> {msg}")
        log_line(">>", msg)

# ==== MAIN LOOP ====
def main():
    print(f"✅ Connected to {SERIAL_PORT}")
    print("Type messages and press Enter to send.")
    print("Press R, T, or C keys directly to send those (case-insensitive).")
    print("Press Ctrl+C to exit.\n")

    # Start serial reading thread
    threading.Thread(target=read_serial, daemon=True).start()

    buffer = ""
    try:
        while True:
            ch = get_char()
            if ch:
                if ch in ['\r', '\n']:
                    if buffer.strip():
                        send_message(buffer.strip())
                    buffer = ""
                    print("> ", end='', flush=True)
                elif ch.lower() in ['r', 't', 'c'] and len(buffer.strip()) == 0:
                    send_message(ch.upper())
                elif ord(ch) == 3:  # Ctrl+C
                    break
                elif ord(ch) == 127:  # Backspace
                    buffer = buffer[:-1]
                    sys.stdout.write("\b \b")
                    sys.stdout.flush()
                else:
                    buffer += ch
                    print(ch, end='', flush=True)
    except KeyboardInterrupt:
        print("\nExiting...")

    finally:
        if ser and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
