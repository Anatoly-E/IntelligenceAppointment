import os
import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import plotly.graph_objects as go
from plotly.subplots import make_subplots


# Корневая директория проекта
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXPORTS_DIR = os.path.join(BASE_DIR, "exports")
FIGURES_DIR = os.path.join(BASE_DIR, "analysis", "figures")
REPORTS_DIR = os.path.join(BASE_DIR, "analysis", "reports")
os.makedirs(FIGURES_DIR, exist_ok=True)
os.makedirs(REPORTS_DIR, exist_ok=True)

class DataAnalyzer:
    def __init__(self, data_file=None):
        if data_file is None:
            data_files = [
                f for f in os.listdir(EXPORTS_DIR)
                if f.startswith("decrypted_data_") and f.endswith(".db")
            ]
            data_file = sorted(data_files)[-1] if data_files else "decrypted_data.db"

        self.db_path = os.path.join(EXPORTS_DIR, data_file)
        self.df = self._load_and_prepare_data()

    # --- 🔧 Подготовка данных ---
    def _load_and_prepare_data(self):
        conn = sqlite3.connect(self.db_path)
        df = pd.read_sql_query("SELECT * FROM sensor_data", conn)
        conn.close()

        df["timestamp"] = pd.to_datetime(df["timestamp"], errors="coerce")
        df["temperature"] = pd.to_numeric(df["temperature"], errors="coerce")
        df["humidity"] = pd.to_numeric(df["humidity"], errors="coerce")
        df["distance"] = pd.to_numeric(df["distance"], errors="coerce")
        df.sort_values("timestamp", inplace=True)

        # 🔹 Заполняем пропущенные значения, чтобы графики не обрывались
        df.interpolate(method="linear", inplace=True)
        df.fillna(method="bfill", inplace=True)
        df.fillna(method="ffill", inplace=True)

        return df

    # --- 📊 Статические графики ---
    def create_static_plots(self):
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        plt.style.use("seaborn-v0_8")

        axes[0, 0].plot(self.df["timestamp"], self.df["temperature"], "r-")
        axes[0, 0].set_title("Температура по времени")
        axes[0, 0].set_ylabel("Температура (°C)")

        axes[0, 1].plot(self.df["timestamp"], self.df["humidity"], "b-")
        axes[0, 1].set_title("Влажность по времени")
        axes[0, 1].set_ylabel("Влажность (%)")

        axes[1, 0].plot(self.df["timestamp"], self.df["distance"], "g-")
        axes[1, 0].set_title("Расстояние по времени")
        axes[1, 0].set_ylabel("Расстояние (см)")

        state_counts = self.df["state"].value_counts()
        axes[1, 1].pie(
            state_counts.values,
            labels=state_counts.index,
            autopct="%1.1f%%",
            startangle=90,
        )
        axes[1, 1].set_title("Распределение состояний системы")

        plt.tight_layout()
        plt.savefig(os.path.join(FIGURES_DIR, "interactive_dashboard.png"), dpi=300)
        plt.close(fig)
        print("[OK] Сохранён общий PNG-график в figures/interactive_dashboard.png")
        print("Уникальные состояния:", self.df["state"].unique())


    # --- 🌐 Интерактивный дашборд ---
    def create_interactive_dashboard(self):

        # Заполняем пропуски, если есть
        self.df.interpolate(method="linear", inplace=True)

        fig = go.Figure()

        # --- Основные графики ---
        fig.add_trace(go.Scatter(
            x=self.df["timestamp"], y=self.df["temperature"],
            mode="lines", name="Температура (°C)",
            line=dict(color="red", width=2),
            yaxis="y1"
        ))

        fig.add_trace(go.Scatter(
            x=self.df["timestamp"], y=self.df["humidity"],
            mode="lines", name="Влажность (%)",
            line=dict(color="blue", width=2, dash="dot"),
            yaxis="y2"
        ))

        fig.add_trace(go.Scatter(
            x=self.df["timestamp"], y=self.df["distance"],
            mode="lines", name="Расстояние (см)",
            line=dict(color="green", width=2, dash="dash"),
            yaxis="y3"
        ))

        # --- Цвета состояний ---
        colors = {
            "off": "rgba(0,255,0,0.25)",      # зелёный
            "standby": "rgba(255,255,0,0.25)", # жёлтый
            "alarm!!!": "rgba(255,0,0,0.25)"   # красный
        }

        # --- Создаём интервалы состояния ---
        df_state = self.df.copy()
        df_state["state_clean"] = df_state["state"].astype(str).str.strip().str.lower()

        prev_state = None
        start_time = None

        for i, row in df_state.iterrows():
            state = row["state_clean"]
            timestamp = row["timestamp"]

            if prev_state is None:
                prev_state = state
                start_time = timestamp
                continue

            # Если состояние изменилось — закрываем интервал
            if state != prev_state:
                end_time = timestamp
                color = colors.get(prev_state, "rgba(150,150,150,0.05)")
                fig.add_vrect(
                    x0=start_time,
                    x1=end_time,
                    fillcolor=color,
                    opacity=0.25,
                    layer="below",
                    line_width=0,
                    annotation_text=prev_state.capitalize(),
                    annotation_position="top left"
                )
                start_time = timestamp
                prev_state = state

        # Закрашиваем последний интервал
        if start_time is not None and prev_state is not None:
            end_time = df_state["timestamp"].iloc[-1]
            color = colors.get(prev_state, "rgba(150,150,150,0.05)")
            fig.add_vrect(
                x0=start_time,
                x1=end_time,
                fillcolor=color,
                opacity=0.25,
                layer="below",
                line_width=0,
                annotation_text=prev_state.capitalize(),
                annotation_position="top left"
            )

        # --- Настройки осей и легенды ---
        fig.update_layout(
            title="📊 Температура, Влажность и Расстояние с подсветкой состояния системы",
            xaxis=dict(title="Время"),
            yaxis=dict(
                title=dict(text="Температура (°C)", font=dict(color="red")),
                tickfont=dict(color="red"),
            ),
            yaxis2=dict(
                title=dict(text="Влажность (%)", font=dict(color="blue")),
                tickfont=dict(color="blue"),
                overlaying="y",
                side="right",
            ),
            yaxis3=dict(
                title=dict(text="Расстояние (см)", font=dict(color="green")),
                tickfont=dict(color="green"),
                overlaying="y",
                side="right",
                anchor="free",
                position=0.98,
            ),
            template="plotly_white",
            height=700,
            legend=dict(x=0.5, y=-0.25, orientation="h", yanchor="bottom", xanchor="center"),
            margin=dict(t=80, b=120)
        )

        # --- Добавляем легенду по состояниям ---
        for name, color in colors.items():
            fig.add_trace(go.Scatter(
                x=[None], y=[None],
                mode="markers",
                marker=dict(size=15, color=color),
                name=name.capitalize()
            ))

        # --- Сохраняем ---
        html_path = os.path.join(REPORTS_DIR, "interactive_dashboard.html")
        # png_path = os.path.join(REPORTS_DIR, "interactive_dashboard.png")
        fig.write_html(html_path)
        # fig.write_image(png_path, scale=2)
        print(f"[OK] Интерактивный дашборд сохранён: {html_path}")


    # --- 🚀 Основной запуск ---
    def run_analysis(self):
        print("[INFO] Анализ данных запущен...")
        self.create_static_plots()
        self.create_interactive_dashboard()
        print("[DONE] Анализ завершён!")


def main():
    analyzer = DataAnalyzer()
    analyzer.run_analysis()


if __name__ == "__main__":
    main()
