#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// SoftAP Credentials
const char* ssid = "BellSystemDemo";
const char* password = "12345678";

ESP8266WebServer server(80);
const int buzzerPin = D6;

unsigned long lastUpdate = 0;
int currentHour = 8;
int currentMinute = 0;
int currentSecond = 0;

struct Event {
  int hour;
  int minute;
  String label;
};

Event schedule[] = {
  {8, 0, "Lesson 1"},
  {8, 10, "Lesson 2"},
  {8, 20, "Break"},
  {8, 30, "Lesson 3"},
  {8, 40, "Lesson 4"},
  {8, 50, "Lunch"},
  {9, 0, "Lesson 5"},
  {9, 10, "Lesson 6"},
  {9, 20, "Free Period"},
  {9, 30, "Lesson 7"},
  {9, 40, "End of Day!"},
};
int scheduleSize = sizeof(schedule) / sizeof(schedule[0]);

#define MAX_CUSTOM_EVENTS 5
Event customEvents[MAX_CUSTOM_EVENTS];
bool customTriggered[MAX_CUSTOM_EVENTS] = {false};
int customEventCount = 0;

bool defaultTriggered[10] = {false};
String currentEvent = "Waiting...";

void ringBell() {
  digitalWrite(buzzerPin, HIGH);
  delay(1500);
  digitalWrite(buzzerPin, LOW);
  Serial.println("🔔 Bell rang!");
}

void updateTime() {
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    currentSecond++;

    if (currentSecond >= 60) {
      currentSecond = 0;
      currentMinute++;

      if (currentMinute >= 60) {
        currentMinute = 0;
        currentHour++;
      }
    }

    if (currentHour >= 18) {
      currentHour = 8;
      currentMinute = 0;
      currentSecond = 0;
      for (int i = 0; i < scheduleSize; i++) defaultTriggered[i] = false;
      for (int i = 0; i < MAX_CUSTOM_EVENTS; i++) customTriggered[i] = false;
      Serial.println("🔁 Schedule reset.");
    }

    if (currentSecond == 0) {
      for (int i = 0; i < scheduleSize; i++) {
        if (currentHour == schedule[i].hour && currentMinute == schedule[i].minute && !defaultTriggered[i]) {
          currentEvent = schedule[i].label;
          ringBell();
          defaultTriggered[i] = true;
        }
      }

      for (int i = 0; i < customEventCount; i++) {
        if (currentHour == customEvents[i].hour && currentMinute == customEvents[i].minute && !customTriggered[i]) {
          currentEvent = customEvents[i].label;
          ringBell();
          customTriggered[i] = true;
        }
      }
    }
  }
}

String formatTime(int hour, int minute, int second = -1) {
  char buf[10];
  if (second >= 0) {
    sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
  } else {
    sprintf(buf, "%02d:%02d", hour, minute);
  }
  return String(buf);
}

Event getNextEvent() {
  int currentTotal = currentHour * 3600 + currentMinute * 60 + currentSecond;
  Event next = {99, 99, "None"};
  int nextTotal = 999999;

  for (int i = 0; i < scheduleSize; i++) {
    int total = schedule[i].hour * 3600 + schedule[i].minute * 60;
    if (total > currentTotal && total < nextTotal) {
      next = schedule[i];
      nextTotal = total;
    }
  }

  for (int i = 0; i < customEventCount; i++) {
    int total = customEvents[i].hour * 3600 + customEvents[i].minute * 60;
    if (total > currentTotal && total < nextTotal) {
      next = customEvents[i];
      nextTotal = total;
    }
  }

  return next;
}

int getSecondsToNext() {
  Event nextEvent = getNextEvent();
  if (nextEvent.hour == 99) return -1;
  
  int nextTotal = nextEvent.hour * 3600 + nextEvent.minute * 60;
  int currentTotal = currentHour * 3600 + currentMinute * 60 + currentSecond;
  
  return nextTotal - currentTotal;
}

String getWebPage() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>🔔 School Bell System</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; color: #333; padding: 20px; }";
  html += ".container { max-width: 900px; margin: 0 auto; background: rgba(255, 255, 255, 0.95); border-radius: 20px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1); overflow: hidden; backdrop-filter: blur(10px); }";
  html += ".header { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); color: white; padding: 30px; text-align: center; }";
  html += ".header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 0 2px 4px rgba(0, 0, 0, 0.2); }";
  html += ".time-display { font-size: 3em; font-weight: 300; margin: 20px 0; text-shadow: 0 2px 4px rgba(0, 0, 0, 0.2); }";
  html += ".content { padding: 30px; }";
  html += ".status-card { background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%); border-radius: 15px; padding: 25px; margin-bottom: 30px; text-align: center; box-shadow: 0 10px 20px rgba(0, 0, 0, 0.1); }";
  html += ".current-event { font-size: 1.8em; color: #2c5530; margin-bottom: 15px; }";
  html += ".next-event { font-size: 1.2em; color: #444; }";
  html += ".countdown { font-size: 2.5em; color: #e74c3c; font-weight: bold; margin: 15px 0; text-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }";
  html += ".schedule-table { background: white; border-radius: 15px; overflow: hidden; box-shadow: 0 10px 20px rgba(0, 0, 0, 0.1); margin-bottom: 30px; }";
  html += ".table-header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; text-align: center; font-size: 1.3em; }";
  html += "table { width: 100%; border-collapse: collapse; }";
  html += "th, td { padding: 15px; text-align: left; border-bottom: 1px solid #eee; }";
  html += "th { background: #f8f9fa; font-weight: 600; color: #555; }";
  html += "tr:hover { background: #f1f3f4; transition: background 0.3s ease; }";
  html += ".custom-event { background: linear-gradient(135deg, #ffecd2 0%, #fcb69f 100%); }";
  html += ".add-event-card { background: white; border-radius: 15px; padding: 30px; box-shadow: 0 10px 20px rgba(0, 0, 0, 0.1); }";
  html += ".add-event-card h3 { color: #333; margin-bottom: 20px; font-size: 1.4em; }";
  html += ".form-group { margin-bottom: 20px; }";
  html += "label { display: block; margin-bottom: 8px; font-weight: 600; color: #555; }";
  html += "input[type='time'], input[type='text'] { width: 100%; padding: 12px 15px; border: 2px solid #e1e5e9; border-radius: 10px; font-size: 1em; transition: border-color 0.3s ease; background: #f8f9fa; }";
  html += "input[type='time']:focus, input[type='text']:focus { outline: none; border-color: #4facfe; background: white; box-shadow: 0 0 0 3px rgba(79, 172, 254, 0.1); }";
  html += ".btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; padding: 15px 30px; border-radius: 10px; font-size: 1.1em; cursor: pointer; transition: transform 0.2s ease, box-shadow 0.2s ease; box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4); }";
  html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 10px 25px rgba(102, 126, 234, 0.6); }";
  html += ".btn:active { transform: translateY(0); }";
  html += ".refresh-btn { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); margin-top: 20px; width: 100%; }";
  html += "@media (max-width: 768px) { .container { margin: 10px; border-radius: 15px; } .header { padding: 20px; } .header h1 { font-size: 2em; } .time-display { font-size: 2.2em; } .content { padding: 20px; } .countdown { font-size: 2em; } }";
  html += ".pulse { animation: pulse 2s infinite; }";
  html += "@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.7; } 100% { opacity: 1; } }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>🔔 School Bell System</h1>";
  html += "<div class='time-display' id='currentTime'>";
  html += formatTime(currentHour, currentMinute, currentSecond);
  html += "</div>";
  html += "</div>";
  html += "<div class='content'>";
  html += "<div class='status-card'>";
  html += "<div class='current-event'>";
  html += "Current: <span style='color: #27ae60;' id='currentEvent'>";
  html += currentEvent;
  html += "</span>";
  html += "</div>";
  html += "<div class='next-event' id='nextEvent'>";
  Event nextEvent = getNextEvent();
  html += "Next: " + formatTime(nextEvent.hour, nextEvent.minute) + " - " + nextEvent.label;
  html += "</div>";
  html += "<div class='countdown pulse' id='countdown'>";
  int secondsToNext = getSecondsToNext();
  if (secondsToNext > 0) {
    int minutes = secondsToNext / 60;
    int seconds = secondsToNext % 60;
    html += String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
  } else {
    html += "00:00";
  }
  html += "</div>";
  html += "</div>";
  html += "<div class='schedule-table'>";
  html += "<div class='table-header'>";
  html += "📅 Today's Schedule";
  html += "</div>";
  html += "<table>";
  html += "<thead>";
  html += "<tr>";
  html += "<th>⏰ Time</th>";
  html += "<th>📚 Event</th>";
  html += "</tr>";
  html += "</thead>";
  html += "<tbody>";

  for (int i = 0; i < scheduleSize; i++) {
    html += "<tr><td>" + formatTime(schedule[i].hour, schedule[i].minute) + "</td><td>" + schedule[i].label + "</td></tr>";
  }
  
  for (int i = 0; i < customEventCount; i++) {
    html += "<tr class='custom-event'><td>" + formatTime(customEvents[i].hour, customEvents[i].minute) + "</td><td>" + customEvents[i].label + " <em>(Custom)</em></td></tr>";
  }

  html += "</tbody>";
  html += "</table>";
  html += "</div>";
  html += "<div class='add-event-card'>";
  html += "<h3>➕ Add Custom Bell</h3>";
  html += "<form action='/add' method='POST'>";
  html += "<div class='form-group'>";
  html += "<label for='time'>⏰ Time:</label>";
  html += "<input type='time' id='time' name='time' required>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label for='label'>📝 Event Title:</label>";
  html += "<input type='text' id='label' name='label' maxlength='30' placeholder='Enter event name...' required>";
  html += "</div>";
  html += "<button type='submit' class='btn'>🔔 Schedule Bell</button>";
  html += "</form>";
  html += "<button onclick='location.reload()' class='btn refresh-btn'>🔄 Refresh Page</button>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // JavaScript section
  html += "<script>";
  html += "let currentHour = " + String(currentHour) + ";";
  html += "let currentMinute = " + String(currentMinute) + ";";
  html += "let currentSecond = " + String(currentSecond) + ";";
  html += "const schedule = [";
  
  for (int i = 0; i < scheduleSize; i++) {
    html += "{hour:" + String(schedule[i].hour) + ",minute:" + String(schedule[i].minute) + ",label:'" + schedule[i].label + "'}";
    if (i < scheduleSize - 1) html += ",";
  }
  
  html += "];";
  html += "const customEvents = [";
  
  for (int i = 0; i < customEventCount; i++) {
    html += "{hour:" + String(customEvents[i].hour) + ",minute:" + String(customEvents[i].minute) + ",label:'" + customEvents[i].label + "'}";
    if (i < customEventCount - 1) html += ",";
  }
  
  html += "];";
  html += "function updateClock() {";
  html += "currentSecond++;";
  html += "if (currentSecond >= 60) { currentSecond = 0; currentMinute++; if (currentMinute >= 60) { currentMinute = 0; currentHour++; } }";
  html += "if (currentHour >= 18) { currentHour = 8; currentMinute = 0; currentSecond = 0; }";
  html += "const timeStr = String(currentHour).padStart(2, '0') + ':' + String(currentMinute).padStart(2, '0') + ':' + String(currentSecond).padStart(2, '0');";
  html += "document.getElementById('currentTime').textContent = timeStr;";
  html += "updateCountdown();";
  html += "}";
  html += "function getNextEvent() {";
  html += "const currentTotal = currentHour * 3600 + currentMinute * 60 + currentSecond;";
  html += "let nextEvent = null;";
  html += "let nextTotal = 999999;";
  html += "for (let event of schedule) { const total = event.hour * 3600 + event.minute * 60; if (total > currentTotal && total < nextTotal) { nextEvent = event; nextTotal = total; } }";
  html += "for (let event of customEvents) { const total = event.hour * 3600 + event.minute * 60; if (total > currentTotal && total < nextTotal) { nextEvent = event; nextTotal = total; } }";
  html += "return nextEvent;";
  html += "}";
  html += "function updateCountdown() {";
  html += "const nextEvent = getNextEvent();";
  html += "const countdownEl = document.getElementById('countdown');";
  html += "if (!nextEvent) { countdownEl.textContent = '00:00'; return; }";
  html += "const nextTotal = nextEvent.hour * 3600 + nextEvent.minute * 60;";
  html += "const currentTotal = currentHour * 3600 + currentMinute * 60 + currentSecond;";
  html += "const secondsToNext = nextTotal - currentTotal;";
  html += "if (secondsToNext <= 0) { countdownEl.textContent = '00:00'; return; }";
  html += "const minutes = Math.floor(secondsToNext / 60);";
  html += "const seconds = secondsToNext % 60;";
  html += "countdownEl.textContent = String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');";
  html += "if (secondsToNext <= 60) { countdownEl.style.color = '#e74c3c'; countdownEl.classList.add('pulse'); } else { countdownEl.style.color = '#e74c3c'; countdownEl.classList.remove('pulse'); }";
  html += "}";
  html += "setInterval(updateClock, 1000);";
  html += "updateCountdown();";
  html += "</script>";
  html += "</body>";

  html += "</html>";

  return html;
}

// API endpoint to get current status (for potential AJAX updates)
void handleStatus() {
  DynamicJsonDocument doc(1024);
  doc["currentHour"] = currentHour;
  doc["currentMinute"] = currentMinute;
  doc["currentSecond"] = currentSecond;
  doc["currentEvent"] = currentEvent;
  
  Event nextEvent = getNextEvent();
  doc["nextEventHour"] = nextEvent.hour;
  doc["nextEventMinute"] = nextEvent.minute;
  doc["nextEventLabel"] = nextEvent.label;
  doc["secondsToNext"] = getSecondsToNext();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleRoot() {
  server.send(200, "text/html", getWebPage());
}

void handleAddEvent() {
  if (server.method() == HTTP_POST) {
    if (customEventCount >= MAX_CUSTOM_EVENTS) {
      server.send(200, "text/html", "<h2>Maximum custom events reached (5).</h2><a href='/'>Go Back</a>");
      return;
    }

    String timeStr = server.arg("time");
    String label = server.arg("label");

    int sep = timeStr.indexOf(':');
    if (sep > 0) {
      int hour = timeStr.substring(0, sep).toInt();
      int minute = timeStr.substring(sep + 1).toInt();

      if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        customEvents[customEventCount] = { hour, minute, label };
        customTriggered[customEventCount] = false;
        customEventCount++;

        Serial.println("✅ Added custom bell: " + label + " at " + String(hour) + ":" + String(minute));
      }
    }

    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  Serial.println("\n📡 Starting SoftAP mode...");
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("🌐 Connect to http://");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/add", handleAddEvent);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("🚀 Server started.");
}

void loop() {
  updateTime();
  server.handleClient();
}
