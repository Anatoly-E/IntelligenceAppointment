import os
import re
import time
import sqlite3
import serial
from datetime import datetime
import serial.tools.list_ports
from crypto_utils import load_or_create_key, encrypt_value

# ---------- Настройки ----------
DB_PATH = os.path.join("data", "data.db")
BAUD_RATE = 115200
LOG_INTERVAL = 0.1 # задержка чтения Serial (сек)

# ---------- Инициализация базы ----------
def init_db():
    os.makedirs("logs", exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            temperature REAL,
            humidity REAL,
            distance REAL,
            state TEXT
        )
    """)
    conn.commit()
    conn.close()

# ---------- Определение COM-порта ----------
def detect_arduino_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "Arduino" in port.description or "USB-SERIAL" in port.description:
            return port.device
    return ports[0].device if ports else None

# ---------- Парсинг строки ----------
def parse_data(line: str):
    """
    Пример входных строк:
    Temperature: 23.4 °C
    Humidity: 40 %
    Distance: 120 sm
    System state: Alarm!!!
    """
    temp = re.search(r"Temperature:\s*([\d.]+)", line)
    hum = re.search(r"Humidity:\s*([\d.]+)", line)
    dist = re.search(r"Distance:\s*([\d.]+)", line)
    state = re.search(r"System state:\s*([\w! ]+)", line)

    return {
        "temperature": float(temp.group(1)) if temp else None,
        "humidity": float(hum.group(1)) if hum else None,
        "distance": float(dist.group(1)) if dist else None,
        "state": state.group(1) if state else None,
    }

# ---------- Запись в базу ----------
def insert_data(data):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        INSERT INTO logs (timestamp, temperature, humidity, distance, state)
        VALUES (?, ?, ?, ?, ?)
    """, (
        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        data["temperature"],
        data["humidity"],
        data["distance"],
        data["state"]
    ))
    conn.commit()
    conn.close()

# ---------- Основной логгер ----------
def log_serial_data(port):
    # Загружаем (или создаём) ключ шифрования
    fernet = load_or_create_key()

    try:
        with serial.Serial(port, BAUD_RATE, timeout=1) as ser:
            print(f"[LOGGER] Подключено к {port}. Запись в {DB_PATH}")
            buffer = ""

            while True:
                line = ser.readline().decode(errors="ignore").strip()
                if line:
                    print(line)
                    buffer += line + " "

                    # Когда накопили блок из нескольких полей (например, строка с состоянием)
                    if "System state" in line:
                        data = parse_data(buffer)

                        # --- 🔐 Шифрование перед записью ---
                        encrypted_data = {}
                        for key, value in data.items():
                            if value is not None:
                                encrypted_data[key] = encrypt_value(fernet, str(value))
                            else:
                                encrypted_data[key] = None

                        # Теперь вместо исходных данных записываем зашифрованные
                        insert_data(encrypted_data)

                        buffer = ""  # очистить буфер после записи

                time.sleep(LOG_INTERVAL)

    except serial.SerialException as e:
        print(f"[LOGGER] Ошибка подключения: {e}")


# ---------- main ----------
def main():
    print("[LOGGER] Инициализация базы данных...")
    init_db()
    print("[LOGGER] Поиск Arduino...")

    port = detect_arduino_port()
    if not port:
        print("[LOGGER] Arduino не найдено.")
        return

    print(f"[LOGGER] Найден порт: {port}")
    log_serial_data(port)


if __name__ == "__main__":
    main()