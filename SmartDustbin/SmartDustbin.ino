/* Smart Dustbin - NodeMCU (ESP8266)

Features:

1. Open on "voice/command" via Telegram message "open"


2. Auto-open when object detected (HC-SR04)


3. Notify via Telegram when bin full (distance < FULL_DIST for N readings)


4. Monitor smell (MQ sensor) & humidity (DHT11) and notify & activate relay (fan)



Pin mapping (change if needed):

SDA -> D2, SCL -> D1 (I2C LCD)

Servo -> D5

Ultrasonic TRIG -> D6, ECHO -> D7 (use voltage divider on ECHO)

DHT11 -> D4

MQ -> A0

Relay1 (fan) -> D8

*/

#include <ESP8266WiFi.h>

#include <WiFiClientSecure.h>

#include <UniversalTelegramBot.h>

#include <ArduinoJson.h>

#include <Servo.h>

#include <DHT.h>

#include <Wire.h>

#include <LiquidCrystal_I2C.h>

#include <NewPing.h>

// ---------- CONFIG ----------

#define WIFI_SSID "Raga"

#define WIFI_PASS "Raga1030"

#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"

#define CHAT_ID "YOUR_CHAT_ID"   // as string

// Pins

#define SERVO_PIN D5

#define DHT_PIN D4

#define MQ_PIN A0

#define RELAY_PIN D8

#define TRIG_PIN D6

#define ECHO_PIN D7

// Hardware constants

#define DHT_TYPE DHT11

#define SERVO_OPEN_ANGLE 85

#define SERVO_CLOSED_ANGLE 0

// Ultrasonic

#define MAX_DISTANCE_CM 200

#define FULL_DIST_CM 12      // when distance reading < this => full

#define FULL_TRIGGER_COUNT 3 // consecutive readings required

// MQ thresholds & humidity

#define MQ_THRESHOLD 400     // calibrate this (analog read)

#define HUMIDITY_ALERT 75.0  // percent

#define HUMIDITY_ALERT_SEC  60UL * 30UL // sustained seconds to trigger (30 min)

// Tele bot check interval

#define TELEGRAM_CHECK_MS 3000

// Debounce times

#define AUTO_CLOSE_MS 5000  // auto-close after open (ms)

#define TELEGRAM_MSG_COOLDOWN_MS 5UL60UL1000UL // don't spam notifications (5 minutes)

// ----------------------------

DHT dht(DHT_PIN, DHT_TYPE);

Servo lidServo;

LiquidCrystal_I2C lcd(0x27, 16, 2); // change address if needed

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);

WiFiClientSecure client;

UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastTelegramCheck = 0;

unsigned long lastAutoClose = 0;

unsigned long lastNotifTime = 0;

unsigned long humidityHighSince = 0;

int fullCounter = 0;

bool lidOpen = false;

bool RELAY_ACTIVE_LOW = false; // set true if your relay triggers on LOW

// Helper to send a Telegram message

void sendTelegram(const String &msg) {

if (millis() - lastNotifTime < TELEGRAM_MSG_COOLDOWN_MS) return;

bot.sendMessage(CHAT_ID, msg, "");

lastNotifTime = millis();

}

// Open lid

void openLid() {

lidServo.write(SERVO_OPEN_ANGLE);

lidOpen = true;

lastAutoClose = millis();

}

// Close lid

void closeLid() {

lidServo.write(SERVO_CLOSED_ANGLE);

lidOpen = false;

}

// Read distance from HC-SR04 (cm)

int readDistance() {

delay(20);

unsigned int uS = sonar.ping();

int cm = uS / US_ROUNDTRIP_CM;

if (cm == 0) return MAX_DISTANCE_CM; // no echo

return cm;

}

void setup() {

Serial.begin(115200);

delay(200);

// LCD

Wire.begin(D2, D1); // SDA, SCL

lcd.init();

lcd.backlight();

lcd.clear();

lcd.setCursor(0,0);

lcd.print("Smart Dustbin");

// Servo

lidServo.attach(SERVO_PIN);

closeLid();

// DHT

dht.begin();

// Relay

pinMode(RELAY_PIN, OUTPUT);

if (RELAY_ACTIVE_LOW) digitalWrite(RELAY_PIN, HIGH);

else digitalWrite(RELAY_PIN, LOW);

// WiFi

WiFi.mode(WIFI_STA);

WiFi.begin(WIFI_SSID, WIFI_PASS);

lcd.setCursor(0,1);

lcd.print("Connecting WiFi");

Serial.print("Connecting WiFi");

int wi = 0;

while (WiFi.status() != WL_CONNECTED) {

delay(500);

Serial.print(".");

lcd.print(".");

wi++;

if (wi > 40) break;

}

if (WiFi.status() == WL_CONNECTED) {

lcd.clear();

lcd.setCursor(0,0);

lcd.print("WiFi connected");

lcd.setCursor(0,1);

lcd.print(WiFi.localIP().toString());

Serial.println();

Serial.print("WiFi IP: ");

Serial.println(WiFi.localIP());

} else {

lcd.clear();

lcd.setCursor(0,0);

lcd.print("WiFi failed");

Serial.println("WiFi failed");

}

// Telegram client - disable cert check (insecure) to simplify

client.setInsecure();

lastTelegramCheck = millis();

}

void loop() {

unsigned long now = millis();

// --- 1) Check Telegram updates (commands) ---

if (now - lastTelegramCheck > TELEGRAM_CHECK_MS) {

int numNew = bot.getUpdates(bot.last_message_received + 1);

for (int i = 0; i < numNew; i++) {

  String text = bot.messages[i].text;

  String fromName = bot.messages[i].from_name;

  String fromId = bot.messages[i].from_id;



  Serial.println("Msg: " + text);

  // Basic commands

  if (text.indexOf("/open") >= 0 || text.indexOf("open") >= 0 || text.indexOf("Open") >= 0) {

    openLid();

    sendTelegram("Dustbin opened (via command).");

  } else if (text.indexOf("/status") >= 0) {

    int dist = readDistance();

    int mq = analogRead(MQ_PIN);

    float hum = dht.readHumidity();

    float temp = dht.readTemperature();

    String s = "Status:\nDistance: " + String(dist) + " cm\nMQ: " + String(mq) + "\nHum: " + String(hum) + "%\nTemp: " + String(temp) + "C";

    sendTelegram(s);

  } else if (text.indexOf("/close") >= 0) {

    closeLid();

    sendTelegram("Dustbin closed (via command).");

  }

}

lastTelegramCheck = now;

}

// --- 2) Ultrasonic auto-open / full detection ---

int distance = readDistance();

lcd.setCursor(0,0);

lcd.print("Dist:");

lcd.print(distance);

lcd.print("cm   ");

if (distance < 60) { // person/object in front => open

openLid();

}

// full detection stability

if (distance <= FULL_DIST_CM) {

fullCounter++;

if (fullCounter >= FULL_TRIGGER_COUNT) {

  sendTelegram("Alert: Dustbin appears FULL (distance " + String(distance) + " cm). Please empty it.");

  fullCounter = 0;

}

} else {

fullCounter = 0;

}

// Auto close after timeout

if (lidOpen && (now - lastAutoClose > AUTO_CLOSE_MS)) {

closeLid();

}

// --- 3) Gas sensor (MQ) smell detection ---

int mqVal = analogRead(MQ_PIN);

lcd.setCursor(0,1);

lcd.print("MQ:");

lcd.print(mqVal);

lcd.print("   ");

if (mqVal >= MQ_THRESHOLD) {

// activate fan via relay

if (RELAY_ACTIVE_LOW) digitalWrite(RELAY_PIN, LOW);

else digitalWrite(RELAY_PIN, HIGH);

sendTelegram("Smell alert: gas level high (" + String(mqVal) + "). Fan ON to disperse.");

delay(60*1000); // keep fan on for 1 minute

// turn fan off

if (RELAY_ACTIVE_LOW) digitalWrite(RELAY_PIN, HIGH);

else digitalWrite(RELAY_PIN, LOW);

}

// --- 4) DHT humidity monitoring (sustained high humidity) ---

float h = dht.readHumidity();

float t = dht.readTemperature();

if (!isnan(h)) {

if (h >= HUMIDITY_ALERT) {

  if (humidityHighSince == 0) humidityHighSince = now;

  else {

    if (now - humidityHighSince >= HUMIDITY_ALERT_SEC * 1000UL) {

      sendTelegram("Alert: High humidity detected for prolonged period (" + String(h) + "%). Possible wet bin/smell.");

      humidityHighSince = now; // reset after sending

    }

  }

} else humidityHighSince = 0;

}

// short delay

delay(200);

}
