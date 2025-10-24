import os
import subprocess
import time

Import("env")

project_dir = env['PROJECT_DIR']
logger_path = os.path.join(project_dir, "scripts", "serial_logger.py")

def after_upload(source, target, env):
    print("[POST-UPLOAD] Прошивка завершена, запуск логгера...")

    # Дадим Arduino 10 секунд на перезапуск после прошивки
    time.sleep(10)

    try:
        # 👉 Здесь используется именно тот Python, который найден у тебя
        subprocess.Popen(
            [r"C:\Users\EAV-Note\.platformio\python3\python.exe", logger_path],
            shell=True
        )
        print(f"[POST-UPLOAD] Логгер запущен: {logger_path}")
    except Exception as e:
        print(f"[POST-UPLOAD] Ошибка запуска логгера: {e}")

# Регистрируем post-upload действие
env.AddPostAction("upload", after_upload)
