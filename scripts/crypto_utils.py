from cryptography.fernet import Fernet
import os

KEY_FILE = os.path.join("secrets", "secret.key")

def load_or_create_key():
    """Создаёт новый или загружает существующий ключ шифрования."""
    # Создаём папку secrets, если её нет
    os.makedirs(os.path.dirname(KEY_FILE), exist_ok=True)

    # Если файл отсутствует или пустой — создаём новый ключ
    if not os.path.exists(KEY_FILE) or os.path.getsize(KEY_FILE) == 0:
        key = Fernet.generate_key()
        with open(KEY_FILE, "wb") as f:
            f.write(key)
        print(f"[CRYPTO] Новый ключ создан: {KEY_FILE}")
        return Fernet(key)

    # Пробуем загрузить существующий ключ
    with open(KEY_FILE, "rb") as f:
        key = f.read().strip()

    # Проверяем корректность
    try:
        return Fernet(key)
    except Exception:
        print("[CRYPTO] Обнаружен повреждённый ключ — пересоздаём...")
        key = Fernet.generate_key()
        with open(KEY_FILE, "wb") as f:
            f.write(key)
        return Fernet(key)

def encrypt_value(fernet, value: str) -> str:
    """Шифрует строку (str → str base64)."""
    if value is None:
        return ""
    return fernet.encrypt(str(value).encode()).decode()

def decrypt_value(fernet, value: str) -> str:
    """Расшифровывает строку (base64 → str)."""
    if not value:
        return ""
    try:
        return fernet.decrypt(value.encode()).decode()
    except Exception:
        return "<DECRYPTION_ERROR>"
