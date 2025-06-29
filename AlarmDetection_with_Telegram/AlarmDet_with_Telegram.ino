#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// WiFi Credentials
const char* ssid = "IDXHUB";
const char* password = "0H6/779w";

// Telegram Bot
#define BOT_TOKEN "7992938995:AAEK3FDx8AScOaQRDSzAE1nRd_o83cDmPfU"
#define CHAT_ID "1708641168"

// Telegram bot setup
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Pins
#define PIR_PIN     13  // D7
#define BUZZER_PIN  14  // D5
#define LED_PIN     0   // D3

// States
bool alarmActive = false;
bool armed = true;  // Alarm is armed by default
unsigned long lastMotionTime = 0;
unsigned long alarmStartTime = 0;
unsigned long lastBotCheck = 0;

const unsigned long alarmDuration = 5000;
const unsigned long cooldownDuration = 10000;
const unsigned long botPollingInterval = 1000;  // 1 second

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  secured_client.setInsecure();  // No SSL cert check

  bot.sendMessage(CHAT_ID, "🟢 Motion detection system is online and *armed*.", "Markdown");
}

void loop() {
  unsigned long currentTime = millis();

  // 🔁 Check for Telegram messages periodically
  if (currentTime - lastBotCheck > botPollingInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    lastBotCheck = currentTime;
  }

  // 🚨 Motion detection logic
  if (armed) {
    int motion = digitalRead(PIR_PIN);

    if (motion == HIGH && !alarmActive && (currentTime - lastMotionTime > cooldownDuration)) {
      Serial.println("🚨 Motion detected! Sending alert...");
      alarmActive = true;
      lastMotionTime = currentTime;
      alarmStartTime = currentTime;

      digitalWrite(LED_PIN, HIGH);

      // Beep 3 times
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
      }

      // Send Telegram Alert
      bot.sendMessage(CHAT_ID, "🚨 *ALERT*: Motion detected!", "Markdown");
    }

    // Reset LED
    if (alarmActive && (currentTime - alarmStartTime > alarmDuration)) {
      digitalWrite(LED_PIN, LOW);
      alarmActive = false;
      Serial.println("✅ Alert reset.");
    }
  }
}

// 📬 Telegram command handling
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.printf("📩 New command from %s: %s\n", from_name.c_str(), text.c_str());

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "🚫 Unauthorized access.", "");
      continue;
    }

    if (text == "/arm") {
      armed = true;
      bot.sendMessage(CHAT_ID, "🔐 System *armed*. Now monitoring for motion.", "Markdown");
    }
    else if (text == "/disarm") {
      armed = false;
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      bot.sendMessage(CHAT_ID, "🔓 System *disarmed*. Motion alerts disabled.", "Markdown");
    }
    else if (text == "/status") {
      String status = armed ? "✅ *Armed*" : "❌ *Disarmed*";
      bot.sendMessage(CHAT_ID, "📊 System Status: " + status, "Markdown");
    }
    else {
      bot.sendMessage(CHAT_ID, "🤖 Available commands:\n/arm\n/disarm\n/status", "");
    }
  }
}
