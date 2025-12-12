#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define trigPin D5
#define echoPin D6
#define servoPin D4
#define relayPin D7

#define DHTPIN D3
#define DHTTYPE DHT11

Servo lidServo;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

long duration;
int distance;

bool lidOpened = false;  // prevents repeated opening

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(relayPin, OUTPUT);

  digitalWrite(relayPin, HIGH);   // Relay OFF (active-low)

  lidServo.attach(servoPin);
  lidServo.write(0);              // Closed position

  dht.begin();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Smart Dustbin");
  delay(1500);
  lcd.clear();
}

void loop() {

  // --- Ultrasonic ---
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // --- DHT ---
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  Serial.print("Distance: "); Serial.print(distance);
  Serial.print(" cm | Temp: "); Serial.print(t);
  Serial.print(" C | Hum: "); Serial.println(h);

  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(t);
  lcd.print(" H:");
  lcd.print(h);

  lcd.setCursor(0,1);
  lcd.print("Dist:");
  lcd.print(distance);
  lcd.print(" cm   ");

  // ✅ Hand detected (<15cm), but only if lid NOT opened yet
  if (distance > 0 && distance < 15 && lidOpened == false) {
    lidOpened = true;  // block repeated opening

    digitalWrite(relayPin, LOW);  // Relay ON
    lidServo.write(90);

    lcd.setCursor(0,1);
    lcd.print("Opening lid    ");
    delay(3000);

    lidServo.write(0);
    digitalWrite(relayPin, HIGH); // Relay OFF

    lcd.setCursor(0,1);
    lcd.print("Closing lid    ");
    delay(1000);
  }

  // ✅ Wait until hand goes away (>15cm) before next trigger
  if (distance > 20) {
    lidOpened = false;
  }

  delay(300);
}
