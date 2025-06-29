#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include <FS.h> // Correct for ESP8266

#define TRIG_PIN D4
#define ECHO_PIN D7
#define LDR_PIN A0
#define SERVO_PIN D8

Servo doorServo;
bool doorOpen = false;
bool manualOverride = false;
bool saidDay = false;
bool saidNight = false;
bool saidAmbient = false;
bool darkMode = false;
int personRepeatCount = 0;

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <style>
    :root {
      --bg-light: linear-gradient(to right, #ece9e6, #ffffff);
      --bg-dark: linear-gradient(to right, #232526, #414345);
      --text-light: #000;
      --text-dark: #fff;
      --card-light: #fff;
      --card-dark: #2c2c2c;
    }
    body {
      font-family: 'Segoe UI', sans-serif;
      background: var(--bg-light);
      text-align: center;
      padding: 20px;
      transition: background 0.3s, color 0.3s;
      color: var(--text-light);
    }
    .dark-mode {
      background: var(--bg-dark);
      color: var(--text-dark);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      max-width: 900px;
      margin: auto;
    }
    .card {
      background: var(--card-light);
      border-radius: 15px;
      padding: 25px;
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.1);
      transition: background 0.3s;
    }
    .dark-mode .card {
      background: var(--card-dark);
    }
    .title {
      font-size: 18px;
    }
    .value {
      font-size: 22px;
      font-weight: bold;
      margin-top: 8px;
    }
    .image-card {
      background: var(--card-light);
      border-radius: 20px;
      padding: 20px;
      margin: 30px auto;
      max-width: 80%;
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.1);
    }
    .image-card img {
      width: 100%;
      height: auto;
      max-height: 500px;
      border-radius: 15px;
    }
    .dark-mode .image-card {
      background: var(--card-dark);
    }
    button {
      margin-top: 30px;
      padding: 12px 25px;
      background-color: #28a745;
      color: white;
      font-size: 16px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    button:hover {
      background-color: #218838;
    }
  </style>
</head>
<body>
  <h2>Smart Home Control Panel</h2>
  <div class="grid">
    <div class="card">
      <div class="title">LDR Reading</div>
      <div class="value" id="ldr">...</div>
    </div>
    <div class="card">
      <div class="title">Distance</div>
      <div class="value" id="distance">...</div>
    </div>
    <div class="card">
      <div class="title">Door Status</div>
      <div class="value" id="door">...</div>
    </div>
    <div class="card">
      <div class="title">Action</div>
      <div class="value" id="action">...</div>
    </div>
  </div>
  <div class="image-card">
    <div class="title">Sign Image</div>
    <img id="signImg" src="" alt="Sign Image">
  </div>

  <button onclick="toggleDoor()">Toggle Door</button>
  <button onclick="toggleDarkMode()" id="modeBtn">Toggle Dark Mode</button>
  <audio id="voice" autoplay></audio>

  <script>
    let currentVoice = "";
    function fetchData() {
      fetch("/data").then(res => res.json()).then(data => {
        document.getElementById("ldr").innerText = data.ldr;
        document.getElementById("distance").innerText = data.distance + " cm";
        document.getElementById("door").innerText = data.door;
        document.getElementById("action").innerText = data.action;
        document.getElementById("signImg").src = data.sign;
        if (data.voice && data.voice !== currentVoice) {
          document.getElementById("voice").src = data.voice;
          currentVoice = data.voice;
        }
      });
    }
    function toggleDoor() {
      fetch("/toggle").then(() => fetchData());
    }
    function toggleDarkMode() {
      document.body.classList.toggle("dark-mode");
      let btn = document.getElementById("modeBtn");
      btn.innerText = document.body.classList.contains("dark-mode") ? "Toggle Light Mode" : "Toggle Dark Mode";
    }
    setInterval(fetchData, 2000);
    fetchData();
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  WiFi.softAP("SmartHome_AP", "12345678");
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.serveStatic("/ambient.mp3", SPIFFS, "/ambient.mp3");
  server.serveStatic("/ambient.png", SPIFFS, "/ambient.png");
  server.serveStatic("/daytime.png", SPIFFS, "/daytime.png");
  server.serveStatic("/nighttime.png", SPIFFS, "/nighttime.png");
  server.serveStatic("/person_near.png", SPIFFS, "/person_near.png");
  server.serveStatic("/daytime.mp3", SPIFFS, "/daytime.mp3");
  server.serveStatic("/nightime.mp3", SPIFFS, "/nightime.mp3");
  server.serveStatic("/person_near.mp3", SPIFFS, "/person_near.mp3");

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    manualOverride = !manualOverride;
    doorOpen = manualOverride;
    doorServo.write(doorOpen ? 120 : 0);
    request->send(200, "text/plain", doorOpen ? "OPEN" : "CLOSED");
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    int ldrVal = analogRead(LDR_PIN);
    long dist = getDistance();
    String action = "Ambient light level";
    String voice = "";
    String sign = "/ambient.png";

    if (ldrVal <= 16 && !saidDay) {
      action = "Daytime - Opening Blinds";
      voice = "/daytime.mp3";
      sign = "/daytime.png";
      saidDay = true;
      saidNight = false;
      saidAmbient = false;
    } else if (ldrVal >= 20 && !saidNight) {
      action = "Nighttime - Closing Blinds";
      voice = "/nightime.mp3";
      sign = "/nighttime.png";
      saidNight = true;
      saidDay = false;
      saidAmbient = false;
    } else if (ldrVal > 16 && ldrVal < 20 && !saidAmbient) {
      action = "Ambient light level";
      voice = "/ambient.mp3";
      sign = "/ambient.png";
      saidAmbient = true;
      saidDay = false;
      saidNight = false;
    } else if (dist > 0 && dist <= 15 && personRepeatCount < 2) {
      action = "Someone is at the door.";
      voice = "/person_near.mp3";
      sign = "/person_near.png";
      personRepeatCount++;
    } else if (dist > 15) {
      personRepeatCount = 0;
    }

    String json = "{";
    json += "\"ldr\":" + String(ldrVal) + ",";
    json += "\"distance\":" + String(dist) + ",";
    json += "\"door\":\"" + String(doorOpen ? "OPEN" : "CLOSED") + "\",";
    json += "\"action\":\"" + action + "\",";
    json += "\"voice\":\"" + voice + "\",";
    json += "\"sign\":\"" + sign + "\"}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  if (!manualOverride) {
    long dist = getDistance();
    if (dist > 0 && dist <= 15 && !doorOpen) {
      doorServo.write(120);
      doorOpen = true;
    } else if ((dist == -1 || dist > 15) && doorOpen) {
      doorServo.write(0);
      doorOpen = false;
    }
  }
  delay(300);
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}
