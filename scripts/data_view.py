import sqlite3
import json
import os
import pandas as pd
import csv
from datetime import datetime
from crypto_utils import load_or_create_key, decrypt_value

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB_PATH = os.path.join(BASE_DIR, "data", "data.db")

class DataViewer:
    def __init__(self):
        self.db_path = DB_PATH
        self.fernet = load_or_create_key()
        
    def decrypt_row_data(self, row):
        """Расшифровка данных строки"""
        id, ts, temp, hum, dist, state = row
        try:
            temp_decrypted = decrypt_value(self.fernet, temp)
            hum_decrypted = decrypt_value(self.fernet, hum)
            dist_decrypted = decrypt_value(self.fernet, dist)
            state_decrypted = decrypt_value(self.fernet, state)
        except Exception:
            # Если данные не зашифрованы (старые записи)
            temp_decrypted = temp
            hum_decrypted = hum
            dist_decrypted = dist
            state_decrypted = state
        
        return {
            'id': id,
            'timestamp': ts,
            'temperature': temp_decrypted,
            'humidity': hum_decrypted,
            'distance': dist_decrypted,
            'state': state_decrypted
        }
    
    def get_all_data(self):
        """Получение всех данных из базы данных"""
        conn = sqlite3.connect(self.db_path)
        cur = conn.cursor()
        cur.execute("SELECT * FROM logs ORDER BY timestamp")
        rows = cur.fetchall()
        conn.close()

        data = []
        for row in rows:
            data_entry = self.decrypt_row_data(row)
            data.append(data_entry)
        
        return data
    
    def export_to_csv(self, data, filename='decrypted_data.csv'):
        """Экспорт в CSV файл"""
        if not data:
            print("Нет данных для экспорта")
            return
        
        # Определяем путь для сохранения
        export_path = os.path.join(BASE_DIR, "exports", filename)
        os.makedirs(os.path.dirname(export_path), exist_ok=True)
        
        # Записываем в CSV
        with open(export_path, 'w', newline='', encoding='utf-8') as csvfile:
            fieldnames = ['id', 'timestamp', 'temperature', 'humidity', 'distance', 'state']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(data)
        
        print(f"Данные экспортированы в {export_path}")
        return export_path
    
    def export_to_excel(self, data, filename='decrypted_data.xlsx'):
        """Экспорт в Excel файл"""
        if not data:
            print("Нет данных для экспорта")
            return
        
        # Определяем путь для сохранения
        export_path = os.path.join(BASE_DIR, "exports", filename)
        os.makedirs(os.path.dirname(export_path), exist_ok=True)
        
        # Преобразуем в DataFrame и сохраняем
        df = pd.DataFrame(data)
        df.to_excel(export_path, index=False, engine='openpyxl')
        print(f"Данные экспортированы в {export_path}")
        return export_path
    
    def export_to_json(self, data, filename='decrypted_data.json'):
        """Экспорт в JSON файл"""
        if not data:
            print("Нет данных для экспорта")
            return
        
        # Определяем путь для сохранения
        export_path = os.path.join(BASE_DIR, "exports", filename)
        os.makedirs(os.path.dirname(export_path), exist_ok=True)
        
        with open(export_path, 'w', encoding='utf-8') as jsonfile:
            json.dump(data, jsonfile, ensure_ascii=False, indent=2)
        print(f"Данные экспортированы в {export_path}")
        return export_path
    
    def save_decrypted_database(self, data, filename='decrypted_data.db'):
        """Создание новой базы данных с расшифрованными данными"""
        if not data:
            print("Нет данных для экспорта")
            return
        
        # Определяем путь для сохранения
        export_path = os.path.join(BASE_DIR, "exports", filename)
        os.makedirs(os.path.dirname(export_path), exist_ok=True)
        
        conn = sqlite3.connect(export_path)
        cursor = conn.cursor()
        
        # Создаем таблицу для расшифрованных данных
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY,
                timestamp TEXT,
                temperature REAL,
                humidity REAL,
                distance REAL,
                state TEXT
            )
        ''')
        
        # Вставляем данные
        for entry in data:
            cursor.execute('''
                INSERT INTO sensor_data 
                (id, timestamp, temperature, humidity, distance, state)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (
                entry['id'],
                entry['timestamp'],
                entry['temperature'],
                entry['humidity'],
                entry['distance'],
                entry['state']
            ))
        
        conn.commit()
        conn.close()
        print(f"Расшифрованная база данных сохранена как {export_path}")
        return export_path
    
    def create_analysis_dataframe(self, data):
        """Создание DataFrame для анализа"""
        if not data:
            return pd.DataFrame()
        
        # Преобразуем данные в DataFrame
        df = pd.DataFrame(data)
        
        # Преобразуем timestamp для анализа
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        df['date'] = df['timestamp'].dt.date
        df['time'] = df['timestamp'].dt.time
        df['hour'] = df['timestamp'].dt.hour
        
        # Преобразуем числовые колонки
        numeric_columns = ['temperature', 'humidity', 'distance']
        for col in numeric_columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')
        
        return df
    
    def generate_report(self, data):
        """Генерация отчета со статистикой"""
        if not data:
            print("Нет данных для отчета")
            return
        
        df = self.create_analysis_dataframe(data)
        
        print("\n" + "="*50)
        print("СТАТИСТИЧЕСКИЙ ОТЧЕТ ДАННЫХ")
        print("="*50)
        
        # Основная статистика
        print(f"Всего записей: {len(data)}")
        print(f"Период данных: с {df['timestamp'].min()} по {df['timestamp'].max()}")
        
        # Статистика по состояниям
        state_stats = df['state'].value_counts()
        print("\nСтатистика по состояниям системы:")
        for state, count in state_stats.items():
            print(f"  {state}: {count} записей")
        
        # Статистика по температуре и влажности
        print(f"\nТемпература:")
        print(f"  Средняя: {df['temperature'].mean():.1f}°C")
        print(f"  Минимальная: {df['temperature'].min():.1f}°C")
        print(f"  Максимальная: {df['temperature'].max():.1f}°C")
        
        print(f"\nВлажность:")
        print(f"  Средняя: {df['humidity'].mean():.1f}%")
        print(f"  Минимальная: {df['humidity'].min():.1f}%")
        print(f"  Максимальная: {df['humidity'].max():.1f}%")
        
        print(f"\nРасстояние:")
        print(f"  Среднее: {df['distance'].mean():.1f}см")
        print(f"  Минимальное: {df['distance'].min():.1f}см")
        print(f"  Максимальное: {df['distance'].max():.1f}см")
        
        # Ежедневная статистика
        daily_stats = df.groupby('date').size()
        if not daily_stats.empty:
            print(f"\nМаксимальная активность в день: {daily_stats.max()} событий")
            print(f"Минимальная активность в день: {daily_stats.min()} событий")
    
    def display_data(self, data, limit=10):
        """Отображение данных в консоли"""
        print("\n=== Расшифрованные данные ===")
        for entry in data[:limit]:
            print(f"[{entry['id']}] {entry['timestamp']} | "
                  f"T={entry['temperature']}°C | H={entry['humidity']}% | "
                  f"D={entry['distance']}cm | {entry['state']}")
        
        if len(data) > limit:
            print(f"\n... и еще {len(data) - limit} записей")
    
    def export_all_formats(self, data=None):
        """Экспорт данных во все форматы"""
        if data is None:
            data = self.get_all_data()
        
        if not data:
            print("Нет данных для экспорта")
            return
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        files = {
            'csv': self.export_to_csv(data, f'decrypted_data_{timestamp}.csv'),
            'excel': self.export_to_excel(data, f'decrypted_data_{timestamp}.xlsx'),
            'json': self.export_to_json(data, f'decrypted_data_{timestamp}.json'),
            'database': self.save_decrypted_database(data, f'decrypted_data_{timestamp}.db')
        }
        
        self.generate_report(data)
        print(f"\nВсе файлы экспортированы с меткой времени: {timestamp}")
        return files
    
    def interactive_menu(self):
        """Интерактивное меню для пользователя"""
        print("Загрузка данных...")
        data = self.get_all_data()
        
        if not data:
            print("Данные не найдены!")
            return
        
        print(f"Загружено {len(data)} записей")
        
        while True:
            print("\n" + "="*50)
            print("МЕНЮ ПРОСМОТРА И ЭКСПОРТА ДАННЫХ")
            print("="*50)
            print("1. Просмотр данных в консоли")
            print("2. Экспорт в CSV")
            print("3. Экспорт в Excel")
            print("4. Экспорт в JSON")
            print("5. Создать расшифрованную базу данных")
            print("6. Сгенерировать статистический отчет")
            print("7. Экспорт во все форматы")
            print("8. Обновить данные")
            print("0. Выход")
            
            choice = input("\nВыберите действие: ").strip()
            
            if choice == '1':
                limit = input("Сколько записей показать (по умолчанию 10): ").strip()
                limit = int(limit) if limit.isdigit() else 10
                self.display_data(data, limit)
            elif choice == '2':
                filename = input("Введите имя файла (по умолчанию: decrypted_data.csv): ").strip()
                self.export_to_csv(data, filename or 'decrypted_data.csv')
            elif choice == '3':
                filename = input("Введите имя файла (по умолчанию: decrypted_data.xlsx): ").strip()
                self.export_to_excel(data, filename or 'decrypted_data.xlsx')
            elif choice == '4':
                filename = input("Введите имя файла (по умолчанию: decrypted_data.json): ").strip()
                self.export_to_json(data, filename or 'decrypted_data.json')
            elif choice == '5':
                filename = input("Введите имя файла (по умолчанию: decrypted_data.db): ").strip()
                self.save_decrypted_database(data, filename or 'decrypted_data.db')
            elif choice == '6':
                self.generate_report(data)
            elif choice == '7':
                self.export_all_formats(data)
            elif choice == '8':
                data = self.get_all_data()
                print(f"Данные обновлены. Загружено {len(data)} записей")
            elif choice == '0':
                break
            else:
                print("Неверный выбор!")

def view_data():
    """Оригинальная функция для обратной совместимости"""
    viewer = DataViewer()
    data = viewer.get_all_data()
    viewer.display_data(data, limit=len(data))

if __name__ == "__main__":
    viewer = DataViewer()
    viewer.interactive_menu()