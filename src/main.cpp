#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <math.h>

// LCD
#define I2C_ADDR 0x27  // Код I2C устройства
#define LCD_COLUMNS 16 // Количество колонок
#define LCD_LINES 2    // Количество строк
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
#define DHTPIN 6      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT22 для эмулятора, DHT 11 для реального устройства
DHT dht(DHTPIN, DHTTYPE);

// Датчик ультразвука
#define ECHO_PIN 9
#define TRIG_PIN 8
#define BUZZER_PIN 7
const int alarmDistance = 50; // см
bool alarmActive = false;
unsigned long alarmStartTime = 0;

// Определяем входы/выходы
const int BUTTON_PIN = 3; // Пульт
const int LED_PIN = 13;   // Электрооборудование
const int GUARD_PIN = 12; // Охранная сигнализация
const int ALARM_PIN = 11; // Сигнал тревоги
const int SERVO_PIN = 5;  // Ворота

// Сервопривод
int maxAngle = 180;                     // Максимальный угол поворота сервопривода
int minAngle = 0;                       // Минимальный угол поворота сервопривода
int step = 1;                           // Шаг поворота
unsigned long lastMoveTime = 0;         // Счётчик для плавного вращения серво
const unsigned long intervalServo = 20; // Пуза в мс между шагами серво для плавного движения
Servo gateServo;

unsigned long previousMillisButton = 0;    // для управления таймером опроса Пульта
unsigned long previousMillisSensor = 0;    // для управления таймером опроса Датчиков
unsigned long previousMillisServo = 0;     // для управления таймером открытия Ворот
const unsigned long intervalButton = 1000; // Интервал между опросами пульта
const long intervalSensor = 2000;          //
int angle = 0;                             // Угол поворота сервопривода
bool gateOpen = true;                      // Открыты ворота или закрыты
bool readyForButton = true;                // кнопка разрешена только в крайних позициях

// Состояния системы
enum GateState
{
  IDLE_OPEN,
  IDLE_CLOSED,
  OPENING,
  CLOSING,
  WAITING_OPEN,
  WAITING_CLOSE,
  ALARM
};
GateState state = IDLE_OPEN;
String currentLCDMessage = "";

// Антидребезг кнопки
bool lastButtonState = HIGH;   // предыдущий логический уровень кнопки
bool stableButtonState = HIGH; // подтверждённое состояние кнопки
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // 50 мс — идеально

// Счётчик
volatile unsigned long tickCount = 0;
float lastTemp = 0;
float lastHum = 0;
float lastDist = 0;

// Опрос датчика температуры
void readDHTTask()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h))
  {
    lastTemp = t;
    lastHum = h;
    Serial.print("Температура: ");
    Serial.print(t);
    Serial.println(" °C");
    Serial.print("Влажность: ");
    Serial.println(h);
  }
  else
  {
    Serial.println("[DHT22] Ошибка чтения!");
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
  long distance = duration / 58;
  return distance;
}

// Обновление экрана
void updateLCD(String text)
{
  if (text != currentLCDMessage)
  {
    currentLCDMessage = text;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(text);
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(lastTemp, 0);
    lcd.print("\1C");
    lcd.print("H:");
    lcd.print(lastHum, 0);
    lcd.print("%");
    lcd.print("D:");
    lcd.print(lastDist, 0);
    lcd.print("cm");
  }
}

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(GUARD_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Сервопривод
  gateServo.attach(SERVO_PIN);
  gateServo.write(angle); // Стартовое положение ворота закрыты

  digitalWrite(LED_PIN, LOW);

  // Запись в Serial Port
  Serial.begin(9600);

  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(1, degree); // Создаем символ под номером 1
  updateLCD("System ready!");

  // Датчик температуры и влажности
  dht.begin();
}

void loop()
{
  // АНТИДРЕБЕЗГ КНОПКИ
  bool rawButtonState = digitalRead(BUTTON_PIN);

  if (rawButtonState != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  lastButtonState = rawButtonState;

  if ((millis() - lastDebounceTime) > debounceDelay)
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
          if (state == CLOSING && state != WAITING_CLOSE)
          {
            state = WAITING_CLOSE;
            updateLCD("Wait! Closing...");
          }
          else if (state == OPENING && state != WAITING_OPEN)
          {
            state = WAITING_OPEN;
            updateLCD("Wait! Opening...");
          }
        }
      }
    }
  }

  // ДВИЖЕНИЕ СЕРВО
  if (millis() - lastMoveTime >= intervalServo)
  {
    lastMoveTime = millis();

    if (gateOpen)
    {
      if (angle < maxAngle)
      {
        if (state != CLOSING && state != WAITING_CLOSE)
        {
          state = CLOSING;
          updateLCD("Closing...");
        }
        angle++;
        gateServo.write(angle);
      }
      else
      {
        if (state != IDLE_CLOSED)
        {
          digitalWrite(LED_PIN, LOW);
          digitalWrite(GUARD_PIN, HIGH);
          digitalWrite(ALARM_PIN, HIGH);
          state = IDLE_CLOSED;
          updateLCD("Closed!");
        }
        readyForButton = true;
      }
    }
    else
    {
      if (angle > minAngle)
      {
        if (state != OPENING && state != WAITING_OPEN)
        {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(GUARD_PIN, LOW);
          state = OPENING;
          updateLCD("Opening...");
        }
        angle--;
        gateServo.write(angle);
      }
      else
      {
        if (state != IDLE_OPEN)
        {
          state = IDLE_OPEN;
          updateLCD("Opened!");
        }
        readyForButton = true;
      }
    }
  }

  // ОПРОС ДАТЧИКОВ
  unsigned long currentMillisSensor = millis();
  if (currentMillisSensor - previousMillisSensor >= intervalSensor)
  {
    previousMillisSensor = currentMillisSensor;
    readDHTTask();
    readDistanceCM();
  }
}
