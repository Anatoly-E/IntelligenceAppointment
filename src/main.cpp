#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// LCD
#define I2C_ADDR    0x27    // Код I2C устройства
#define LCD_COLUMNS 16      // Количество колонок
#define LCD_LINES   2       // Количество строк
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

byte degree[8] =            // Кодируем символ градуса
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
#define DHTPIN 6            // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22       // ВРЕ22 для эмулятора, DHT 11 для реального устройства
DHT dht(DHTPIN, DHTTYPE);

// Датчик ультразвука
//#define ECHO_PIN 2
//#define TRIG_PIN 3

// Определяем входы/выходы
const int BUTTON_PIN = 8;   // Пульт
const int LED_PIN = 13;     // Электрооборудование
const int GUARD_PIN = 11;   // Охранная сигнализация
const int ALARM_PIN = 12;   // Сигнал тревоги
const int SERVO_PIN = 5;    // Ворота

// Сервопривод
Servo gateServo;

bool gateOpen = false;
unsigned long lastActionTime = 0;
const unsigned long cooldown = 1000; // 1 секунда
const unsigned long servoRunTime = 5000; // 5 секунд

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(GUARD_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  //pinMode(TRIG_PIN, OUTPUT);
  //pinMode(ECHO_PIN, INPUT);

  // Сервопривод
  gateServo.attach(SERVO_PIN);
  gateServo.write(180); // Стартовое положение ворота закрыты
  
  digitalWrite(LED_PIN, LOW);
  
  // Запись в Serial Port
  Serial.begin(115200);
  Serial.println("System ready");
  
  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.print("System ready!");
  lcd.createChar(1, degree); // Создаем символ под номером 1

  // Датчик температуры и влажности
  dht.begin();
}

float temperature () {
        // Reading temperature or humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
      float h = dht.readHumidity();
      // Read temperature as Celsius (the default)
      float t = dht.readTemperature();
      // Read temperature as Fahrenheit (isFahrenheit = true)
      float f = dht.readTemperature(true);

      // Compute heat index in Fahrenheit (the default)
      // float hif = dht.computeHeatIndex(f, h);
      // Compute heat index in Celsius (isFahreheit = false)
      //float hic = dht.computeHeatIndex(t, h, false);

      // Serial.print("Humidity: ");
      // Serial.print(h);
      // Serial.print("%  Tempeature: ");
      // Serial.print(t);
      // Serial.print("°C ");

      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t) || isnan(f)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return 0;
      } else {
        return t;
      }

}

//float readDistanceCM() {
//  digitalWrite(TRIG_PIN, LOW);
//  delayMicroseconds(2);
//  digitalWrite(TRIG_PIN, HIGH);
//  delayMicroseconds(10);
//  digitalWrite(TRIG_PIN, LOW);
//  int duration = pulseIn(ECHO_PIN, HIGH);
//  return duration * 0.034 / 2;
//}

void loop() {
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);

  // обработка нажатия
  if (lastButtonState == HIGH && buttonState == LOW) {
    unsigned long now = millis();
    if (now - lastActionTime >= cooldown) {
      lastActionTime = now;

      if (!gateOpen) {
        // открытие
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(GUARD_PIN, LOW);
        //Serial.println("Opening...");
        //lcd.begin(16, 2);
        lcd.clear();
        lcd.print("Opening...");
        gateServo.write(0);
        for (int i = 180; i >= 0; i-- ) {
          gateServo.write(i);
          delay(20);
        }
        delay(servoRunTime);
        
        // Процесс открытия завершён
        //lcd.begin(16, 2);
        lcd.clear();
        lcd.print("Opened!");
        lcd.setCursor(0, 1);
        lcd.print("Temp =     \1C "); // \1 - значок градуса
        lcd.setCursor(7, 1);          // Устанавливаем курсор на 7 символ
        lcd.print(temperature(), 1);

        gateOpen = true;
      } else {
        // закрытие
        //Serial.println("Closing...");
        //lcd.begin(16, 2);
        lcd.clear();
        lcd.print("Closing...");
        for (int i = 0; i <= 180; i++ ) {
          gateServo.write(i);
          delay(20);

        }
        delay(servoRunTime);

        // Процесс закрытия завершён
        digitalWrite(LED_PIN, LOW);
        digitalWrite(GUARD_PIN, HIGH);
        //lcd.begin(16, 2);
        lcd.clear();
        lcd.print("Closed!");
        lcd.setCursor(0, 1);
        lcd.print("Temp =     \1C "); // \1 - значок градуса
        lcd.setCursor(7, 1);          // Устанавливаем курсор на 7 символ
        lcd.print(temperature(), 1);

        //float distance = readDistanceCM();
        //bool isNearby = distance < 100;
        //digitalWrite(ALARM_PIN, isNearby);
        //Serial.print("Measured distance: ");
        //Serial.println(readDistanceCM());


        gateOpen = false;
      }

      //gateServo.write(90); // возвращаем в нейтраль
    } else {
      //Serial.println("Cooldown active...");
      lcd.clear();
      lcd.print("Cooldown active...");

    }
  }

  lastButtonState = buttonState;
}
