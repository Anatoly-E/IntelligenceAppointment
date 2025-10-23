#include <Arduino.h>
#include <Servo.h> // Для эмулятора использовать бибилиотеку <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

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

// Определяем входы/выходы
const int BUTTON_PIN = 3; // Пульт
const int LED_PIN = 13;   // Электрооборудование
const int GUARD_PIN = 11; // Охранная сигнализация
const int ALARM_PIN = 12; // Сигнал тревоги
const int SERVO_PIN = 5;  // Ворота

// Сервопривод
int currentAngle = 180; // Текущий угол сервопривода
int targetAngle = 90;   // Целевой угол
int step = 1;           // Шаг поворота
Servo gateServo;

bool gateOpen = false;                   // Открыты ворота или закрыты
unsigned long previousMillisButton = 0;  // для управления таймером опроса Пульта
unsigned long previousMillisSensor = 0;  // для управления таймером опроса Датчиков
unsigned long previousMillisServo = 0;   // для управления таймером открытия Ворот
const unsigned long cooldown = 1000;     // Интервал между опросами пульта
const unsigned long servoRunTime = 5000; // Интервал открытия ворот
const long intervalSensor = 2000;
int angle = 0; // интервал в миллисекундах

// Счётчик
volatile unsigned long tickCount = 0;
float lastTemp = 0;
float lastHum = 0;
float lastDist = 0;
volatile bool flagTimer = false;

void readDHTTask() // Опрос датчика температуры
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

void readUltrasonicTask() // Опрос УЗ датчика
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.034 / 2;

  if (distance > 0 && distance < 400)
  {
    lastDist = distance;
  }
}

// Функция обновления экрана
void updateLCD(String text)
{
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

// Прерывание таймера (1 Гц)
// ISR(TIMER1_COMPA_vect)
// {
//   tickCount++;
//   flagTimer = true;
//   // readUltrasonicTask();
//   // readDHTTask(); // Каждую секунду
//   if (tickCount % 2 == 0)
//   { // Каждые 2 секунды
//   }

//   if (tickCount % 2 == 0)
//   { // Обновляем LCD каждые 2 секунды
//     // updateLCD();
//   }
// }

// void setupTimer1()
// {
//   noInterrupts();
//   TCCR1A = 0;
//   TCCR1B = 0;
//   TCNT1 = 0;
//   OCR1A = 15624; // 1 Гц (16 MHz / 1024 / 1Hz - 1)
//   TCCR1B |= (1 << WGM12);
//   TCCR1B |= (1 << CS12) | (1 << CS10); // делитель 1024
//   TIMSK1 |= (1 << OCIE1A);
//   interrupts();
// }

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(GUARD_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Сервопривод
  gateServo.attach(SERVO_PIN);
  gateServo.write(currentAngle); // Стартовое положение ворота закрыты

  digitalWrite(LED_PIN, LOW);

  // Запись в Serial Port
  // Serial.begin(115200);
  Serial.begin(9600);
  // Serial.println("System ready");

  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(1, degree); // Создаем символ под номером 1
  updateLCD("System ready!");

  // Датчик температуры и влажности
  dht.begin();
  // Прикрепление функции `readDHTTask` к прерыванию на пине
  // attachInterrupt(digitalPinToInterrupt(DHTPIN), readDHTTask, RISING);

  // setupTimer1();
}

void loop()
{
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);
  unsigned long currentlMillisSensor = millis();
  // unsigned long servoMillis = millis();
  // unsigned long currentMillisServo = millis();

  // if (flagTimer)
  // {
  //   flagTimer = false;
  // }

  // Выполнение действия с интервалом опроса датчиков
  if (currentlMillisSensor - previousMillisSensor >= intervalSensor)
  {
    previousMillisSensor = currentlMillisSensor;

    readDHTTask();
    readUltrasonicTask();
  }

  // обработка нажатия
  if (lastButtonState == HIGH && buttonState == LOW)
  {
    unsigned long currentMillisButton = millis();

    if (currentMillisButton - previousMillisButton >= cooldown)
    {
      previousMillisButton = currentMillisButton;

      if (!gateOpen)
      {
        // открытие
        currentAngle = 180; // Текущий угол сервопривода
        targetAngle = 0;    // Целевой угол
        unsigned long currentMillisServo = millis();

        if (currentMillisServo - previousMillisServo >= 1000)
        {
          previousMillisServo = currentMillisServo;

          if (currentAngle != targetAngle)
          {
            if (currentAngle < targetAngle)
            {
              currentAngle += step;
            }
            else
            {
              currentAngle -= step;
            }
            gateServo.write(currentAngle);
          }

        }


        digitalWrite(LED_PIN, HIGH);
        digitalWrite(GUARD_PIN, LOW);
        updateLCD("Opening...");
        // gateServo.write(0);
        //  for (int i = 180; i >= 0; i--)
        //  {
        //    gateServo.write(i);
        //    delay(20);
        //  }
        // delay(servoRunTime);

        // Процесс открытия завершён
        updateLCD("Opened!");

        gateOpen = true;
      }
      else
      {
        // закрытие
        // Serial.println("Closing...");

        currentAngle = 0;  // Текущий угол сервопривода
        targetAngle = 180; // Целевой угол

        if (currentAngle != targetAngle)
        {
          if (currentAngle < targetAngle)
          {
            currentAngle += step;
          }
          else
          {
            currentAngle -= step;
          }
          gateServo.write(currentAngle);
        }

        updateLCD("Closing...");
        // for (int i = 0; i <= 180; i++)
        // {
        //   gateServo.write(i);
        //   delay(20);
        // }
        // delay(servoRunTime);

        // Процесс закрытия завершён
        digitalWrite(LED_PIN, LOW);
        digitalWrite(GUARD_PIN, HIGH);
        updateLCD("Closed!");

        gateOpen = false;
      }

      // gateServo.write(90); // возвращаем в нейтраль
    }
    else
    {
      // Serial.println("Cooldown active...");
      updateLCD("Cooldown active...");
    }
  }

  lastButtonState = buttonState;
}
