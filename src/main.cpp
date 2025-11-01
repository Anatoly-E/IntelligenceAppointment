#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Настройка системы
constexpr unsigned long ALARM_DURATION = 3000;  // Длительность звуковой сигнализации (мс)
constexpr int ALARM_DISTANCE = 50;             // Порог дистанции срабатывания охранной сигнализации (см)
constexpr unsigned long INTERVAL_SERVO = 20;    // Пауза между шагами серво для плавного движения (мс)
constexpr unsigned long INTERVAL_SENSOR = 2000; // Интервал опроса датчиков (мс)

// LCD
constexpr uint8_t I2C_ADDR = 0x27;  // Код I2C устройства
constexpr uint8_t LCD_COLUMNS = 16; // Количество колонок
constexpr uint8_t LCD_LINES = 2;    // Количество строк
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

byte degree[8] = // Кодируем символ градуса
    {
        B00111,
        B00101,
        B00111,
        B00000,
        B00000,
        B00000,
        B00000,
};

// DHT Сенсор
constexpr uint8_t DHTPIN = 6;      // Digital pin connected to the DHT sensor
constexpr uint8_t DHTTYPE = DHT11; // DHT22 для эмулятора, DHT 11 для реального устройства
DHT dht(DHTPIN, DHTTYPE);

// Датчик ультразвука
constexpr uint8_t ECHO_PIN = 8;
constexpr uint8_t TRIG_PIN = 9;
constexpr uint8_t BUZZER_PIN = 7;

// Параметры для охранной сигнализации
unsigned long alarmStartTime = 0;
bool alarmActive = false;
enum SystemState
{
  SYSTEM_ALARM,
  SYSTEM_OK,
  SYSTEM_STANDBY
};
SystemState sysState = SystemState::SYSTEM_OK;

// Функция возвращает текст по значению состояния
const char *getSystemStateText(SystemState state)
{
  switch (state)
  {
  case SystemState::SYSTEM_ALARM:
    return "Alarm!!!";
  case SystemState::SYSTEM_OK:
    return "OFF";
  case SystemState::SYSTEM_STANDBY:
    return "Standby";
  default:
    return "Unknown";
  }
}

// Определяем входы/выходы
constexpr uint8_t BUTTON_PIN = 3; // Пульт
constexpr uint8_t LED_PIN = 13;   // Электрооборудование
constexpr uint8_t GUARD_PIN = 12; // Охранная сигнализация
constexpr uint8_t ALARM_PIN = 11; // Сигнал тревоги
constexpr uint8_t SERVO_PIN = 5;  // Ворота

// Сервопривод
constexpr int maxAngle = 180;   // Максимальный угол поворота сервопривода
constexpr int minAngle = 0;     // Минимальный угол поворота сервопривода
constexpr int step = 1;         // Шаг поворота
unsigned long lastMoveTime = 0; // Счётчик для плавного вращения серво
Servo gateServo;

// Переменные
unsigned long previousMillisButton = 0; // для управления таймером опроса Пульта
unsigned long previousMillisSensor = 0; // для управления таймером опроса Датчиков
unsigned long previousMillisServo = 0;  // для управления таймером открытия Ворот
int angle = 0;                          // Угол поворота сервопривода
bool gateOpen = false;                  // Открыты ворота или закрыты
bool readyForButton = true;             // кнопка разрешена только в крайних позициях

// Состояния системы
enum class GateState
{
  IDLE_OPEN,
  IDLE_CLOSED,
  OPENING,
  CLOSING,
  WAITING_OPEN,
  WAITING_CLOSE,
  ALARM
};
GateState gateState = GateState::IDLE_OPEN;

// Антидребезг кнопки
bool lastButtonState = HIGH;                 // предыдущий логический уровень кнопки
bool stableButtonState = HIGH;               // подтверждённое состояние кнопки
unsigned long lastDebounceTime = 0;          // для опроса кнопки
constexpr unsigned long DEBOUNCE_DELAY = 50; // 50 мс — идеально

// Отслеживаемые датчики
float lastTemp = 0.0f; // Температура
float lastHum = 0.0f;  // Влажность
float lastDist = 0.0f; // Расстояние

// Опрос датчика температуры
void readDHTTask()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h))
  {
    lastTemp = t;
    lastHum = h;
  }
  else
  {
    // Serial.println("[DHT22] Read error!");
  }
}

// Опрос УЗ датчика
long readDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 20000); // таймаут ~20ms
  lastDist = duration / 58;

  return lastDist;
}

// Обновление экрана
void updateLCD(const String &text, SystemState systemState)
{

  static String lastText = "";

  if (text != lastText)
  {
    lastText = text;
    lcd.clear();
  }

  switch (systemState)
  {
  case SYSTEM_OK:
    lcd.setCursor(0, 0);
    lcd.print(text);
    lcd.setCursor(0, 1);
    lcd.print(F("T:"));
    lcd.print(lastTemp, 0);
    lcd.print("\1C H:");
    lcd.print(lastHum, 0);
    lcd.print("%");
    break;

  case SYSTEM_STANDBY:
    lcd.setCursor(0, 0);
    lcd.print(text);
    lcd.setCursor(0, 1);
    lcd.print(F("T:"));
    lcd.print(lastTemp, 0);
    lcd.print("\1C H:");
    lcd.print(lastHum, 0);
    lcd.print("%");
    break;

  case SYSTEM_ALARM:
    lcd.setCursor(0, 0);
    lcd.print(text);
    lcd.setCursor(0, 1);
    lcd.print(F("Distance: "));
    lcd.print(lastDist, 0);
    lcd.print("sm");
    break;
  }
}

void updateSerial(float _currentTemp, float _currentHumidity, float _currentDistance, SystemState currentSystemState)
{

  float currentTemp =  !isnan(_currentTemp) ? _currentTemp : 0;
  float currentHumidity =  !isnan(_currentHumidity) ? _currentHumidity : 0;
  float currentDistance =  !isnan(_currentDistance) ? _currentDistance : 0;

  Serial.print("Temperature: ");
  Serial.print(currentTemp);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(currentHumidity);
  Serial.println(" %");
  Serial.print(F("Distance: "));
  Serial.print(currentDistance);
  Serial.println(F(" sm"));
  Serial.print(F("System state: "));
  Serial.println(getSystemStateText(currentSystemState));
}

// Охранная сигнализация
void checkAlarm()
{
  if (gateState != GateState::IDLE_CLOSED)
  {
    sysState = SYSTEM_OK;
    digitalWrite(ALARM_PIN, LOW);
    noTone(BUZZER_PIN);
    alarmActive = false;
    return;
  }

  sysState = SYSTEM_STANDBY;
  readDistanceCM();

  if (lastDist > 0 && lastDist < ALARM_DISTANCE)
  {
    sysState = SYSTEM_ALARM;
    if (!alarmActive)
    {
      alarmActive = true;
      alarmStartTime = millis();
      digitalWrite(ALARM_PIN, HIGH);
      tone(BUZZER_PIN, 2000);
    }
    updateLCD(F("Alarm!"), sysState);
  }
  else if (alarmActive && (millis() - alarmStartTime >= ALARM_DURATION))
  {
    noTone(BUZZER_PIN);
    alarmActive = false;
    sysState = SYSTEM_STANDBY;
  }
}

void setup()
{
  digitalWrite(LED_PIN, LOW);
  digitalWrite(GUARD_PIN, LOW);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(GUARD_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Сервопривод
  gateServo.attach(SERVO_PIN);
  gateServo.write(minAngle);
  gateState = GateState::IDLE_OPEN;

  // Запись в Serial Port
  Serial.begin(115200);

  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(1, degree); // Создаем символ под номером 1

  // Датчик температуры и влажности
  dht.begin();

  // Проверка УЗ и установка начального значения
  readDHTTask();
  readDistanceCM();

  // Инициализация - ворота открыты
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(GUARD_PIN, LOW);
  digitalWrite(ALARM_PIN, LOW);
  gateServo.write(minAngle);

  updateLCD("System ready!", SYSTEM_OK);
}

void loop()
{
  // Антидребезг кнопки
  bool rawButtonState = digitalRead(BUTTON_PIN);

  if (rawButtonState != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  lastButtonState = rawButtonState;

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {
    if (rawButtonState != stableButtonState)
    {
      stableButtonState = rawButtonState;

      // Кнопка нажата
      if (stableButtonState == LOW)
      {
        if (readyForButton)
        {
          gateOpen = !gateOpen;
          readyForButton = false;
        }
        else
        {
          // Кнопка нажата во время движения → показываем статус ожидания
          if (gateState == GateState::CLOSING && gateState != GateState::WAITING_CLOSE)
          {
            gateState = GateState::WAITING_CLOSE;
            updateLCD("Wait! Closing...", SYSTEM_OK);
          }
          else if (gateState == GateState::OPENING && gateState != GateState::WAITING_OPEN)
          {
            gateState = GateState::WAITING_OPEN;
            updateLCD("Wait! Opening...", SYSTEM_OK);
          }
        }
      }
    }
  }

  // ДВИЖЕНИЕ СЕРВО
  if (millis() - lastMoveTime >= INTERVAL_SERVO)
  {
    lastMoveTime = millis();

    if (gateOpen)
    {
      if (angle < maxAngle)
      {
        // Процесс закрытия
        if (gateState != GateState::CLOSING && gateState != GateState::WAITING_CLOSE)
        {
          gateState = GateState::CLOSING;
          updateLCD("Closing...", SYSTEM_OK);
        }
        angle++;
        gateServo.write(angle);
      }
      else
      {
        // Закрыты
        if (gateState != GateState::IDLE_CLOSED)
        {
          digitalWrite(LED_PIN, LOW);
          digitalWrite(GUARD_PIN, HIGH);
          gateState = GateState::IDLE_CLOSED;
          updateLCD("Closed!", SYSTEM_OK);
        }
        readyForButton = true;
      }
    }
    else
    {
      if (angle > minAngle)
      {
        // Процесс открытия
        if (gateState != GateState::OPENING && gateState != GateState::WAITING_OPEN)
        {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(GUARD_PIN, LOW);

          gateState = GateState::OPENING;
          updateLCD("Opening...", SYSTEM_OK);
        }
        angle--;
        gateServo.write(angle);
      }
      else
      {
        // Открыты
        if (gateState != GateState::IDLE_OPEN)
        {
          gateState = GateState::IDLE_OPEN;
          updateLCD("Opened!", SYSTEM_OK);
        }
        readyForButton = true;
      }
    }
  }

  // Опрос датчиков и охранной сигнализации
  if (millis() - previousMillisSensor >= INTERVAL_SENSOR)
  {
    previousMillisSensor = millis();
    readDHTTask();
    checkAlarm();
    updateSerial(lastTemp, lastHum, lastDist, sysState);
  }
}
