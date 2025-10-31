import os
import time
import serial
from datetime import datetime

def detect_arduino_port():
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        for port in ports:
            if "Arduino" in port.description or "USB-SERIAL" in port.description:
                return port.device
        if ports:
            return ports[0].device
    except Exception:
        pass
    return None

def log_serial_data(port, baud=115200, log_file="logs/log.txt", duration=None):
    try:
        with serial.Serial(port, baud, timeout=1) as ser, open(log_file, "a", encoding="utf-8") as f:
            print(f"[LOGGER] Подключено к {port}, запись идёт...")
            start = time.time()
            while True:
                if duration and (time.time() - start > duration):
                    print("[LOGGER] Время записи истекло, завершение.")
                    break
                line = ser.readline().decode(errors="ignore").strip()
                if line:
                    timestamp = datetime.now().strftime("[%Y-%m-%d %H:%M:%S]")
                    entry = f"{timestamp} {line}\n"
                    f.write(entry)
                    f.flush()
                    print(entry, end="")
                time.sleep(0.1)
    except serial.SerialException:
        print("[LOGGER] Ошибка подключения. Возможно, порт занят или Arduino не подключено.")

def main():
    print("[LOGGER] Запуск логгера...")
    time.sleep(2)  # ждём, пока Arduino перезагрузится

    port = detect_arduino_port()
    if port:
        print(f"[LOGGER] Найден порт: {port}")
        log_serial_data(port, duration=None)
    else:
        print("[LOGGER] Arduino не обнаружено.")

if __name__ == "__main__":
    main()
