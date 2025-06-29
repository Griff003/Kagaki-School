#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

// === Pins ===
#define IR_SENSOR D5
#define SERVO_PIN D7
#define SDA_PIN D2
#define SCL_PIN D1

// === LCD setup ===
const byte possibleAddresses[] = {0x27, 0x3F};
LiquidCrystal_I2C* lcd = nullptr;

// === Feeding times (in ms for demo) ===
unsigned long feedTime1 = 10000;
unsigned long feedTime2 = 30000;
unsigned long feedTime3 = 50000;

bool fed1 = false;
bool fed2 = false;
bool fed3 = false;
bool irFed = false;
bool irAllowed = true;
unsigned long startTime;

Servo feederServo;

void setup() {
  Serial.begin(9600);
  Serial.println("Booting Pet Feeder...");

  // Start I2C on custom SDA/SCL
  Wire.begin(SDA_PIN, SCL_PIN);
  byte foundAddress = 0;

  // Auto-detect LCD address
  for (byte i = 0; i < sizeof(possibleAddresses); i++) {
    Wire.beginTransmission(possibleAddresses[i]);
    if (Wire.endTransmission() == 0) {
      foundAddress = possibleAddresses[i];
      Serial.print("LCD found at address 0x");
      Serial.println(foundAddress, HEX);
      break;
    }
  }

  if (foundAddress == 0) {
    Serial.println("LCD not found. Check connections.");
    while (true); // Halt
  }

  lcd = new LiquidCrystal_I2C(foundAddress, 16, 2);
  lcd->begin(16, 2);
  lcd->backlight();
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Feeder Ready");

  // Init components
  pinMode(IR_SENSOR, INPUT);
  feederServo.attach(SERVO_PIN);
  feederServo.write(0); // Close

  startTime = millis();
  delay(2000);
  lcd->clear();
}

void dispenseFood(String reason) {
  feederServo.write(150);  // Open
  delay(1000);
  feederServo.write(0);   // Close

  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print(reason + " Fed");

  Serial.println(reason + " Fed at " + String((millis() - startTime) / 1000) + "s");
}

void loop() {
  unsigned long now = millis();
  int irStatus = digitalRead(IR_SENSOR);

  if (!fed1 && now - startTime >= feedTime1) {
    dispenseFood("Meal1");
    fed1 = true;
  }

  if (!fed2 && now - startTime >= feedTime2) {
    dispenseFood("Meal2");
    fed2 = true;
  }

  if (!fed3 && now - startTime >= feedTime3) {
    dispenseFood("Meal3");
    fed3 = true;
  }

  if (irStatus == LOW && irAllowed && !irFed) {
    Serial.println("IR triggered");
    dispenseFood("Treat");
    irFed = true;
    irAllowed = false;
  }

  delay(100);
}
