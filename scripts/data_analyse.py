import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

class DataAnalyzer:
    def __init__(self, data_file=None):
        if data_file is None:
            # Ищем последний файл данных
            exports_dir = os.path.join(BASE_DIR, "exports")
            data_files = [f for f in os.listdir(exports_dir) if f.startswith('decrypted_data_') and f.endswith('.db')]
            if data_files:
                data_file = sorted(data_files)[-1]  # Берем самый новый
            else:
                data_file = 'decrypted_data.db'
        
        self.db_path = os.path.join(BASE_DIR, "exports", data_file)
        self.df = self.load_data()
    
    def load_data(self):
        """Загрузка данных из расшифрованной БД"""
        conn = sqlite3.connect(self.db_path)
        df = pd.read_sql_query("SELECT * FROM sensor_data", conn)
        conn.close()
        
        # Преобразуем типы данных
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        df['temperature'] = pd.to_numeric(df['temperature'], errors='coerce')
        df['humidity'] = pd.to_numeric(df['humidity'], errors='coerce')
        df['distance'] = pd.to_numeric(df['distance'], errors='coerce')
        
        return df
    
    def create_basic_plots(self):
        """Создание базовых графиков"""
        # Настройка стиля
        plt.style.use('seaborn-v0_8')
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        
        # График температуры по времени
        axes[0, 0].plot(self.df['timestamp'], self.df['temperature'], 'r-', alpha=0.7)
        axes[0, 0].set_title('Температура по времени')
        axes[0, 0].set_ylabel('Температура (°C)')
        axes[0, 0].tick_params(axis='x', rotation=45)
        
        # График влажности по времени
        axes[0, 1].plot(self.df['timestamp'], self.df['humidity'], 'b-', alpha=0.7)
        axes[0, 1].set_title('Влажность по времени')
        axes[0, 1].set_ylabel('Влажность (%)')
        axes[0, 1].tick_params(axis='x', rotation=45)
        
        # График расстояния по времени
        axes[1, 0].plot(self.df['timestamp'], self.df['distance'], 'g-', alpha=0.7)
        axes[1, 0].set_title('Расстояние по времени')
        axes[1, 0].set_ylabel('Расстояние (см)')
        axes[1, 0].tick_params(axis='x', rotation=45)
        
        # Статистика по состояниям
        state_counts = self.df['state'].value_counts()
        axes[1, 1].pie(state_counts.values, labels=state_counts.index, autopct='%1.1f%%')
        axes[1, 1].set_title('Распределение состояний системы')
        
        plt.tight_layout()
        plt.savefig(os.path.join(BASE_DIR, 'exports', 'basic_analysis.png'), dpi=300, bbox_inches='tight')
        plt.show()
    
    # ... остальные методы без изменений

def main():
    analyzer = DataAnalyzer()
    
    print("Создание базовых графиков...")
    analyzer.create_basic_plots()
    
    print("Создание продвинутых графиков...")
    analyzer.create_advanced_plots()
    
    print("Генерация HTML отчета...")
    analyzer.generate_html_report()
    
    print("Анализ завершен!")

if __name__ == "__main__":
    main()