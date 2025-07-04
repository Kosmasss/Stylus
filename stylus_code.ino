#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MPU and web server
Adafruit_MPU6050 mpu;
WebServer server(80);

// Gyro data deltas
float gyroDeltaX = 0;
float gyroDeltaY = 0;

// Tracking state
bool recording = false;

// Button setup
const int buttonPin = 25;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void updateGyro() {
  if (!recording) {
    gyroDeltaX = 0;
    gyroDeltaY = 0;
    return;
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  gyroDeltaX = g.gyro.x * 10;  // Scaled for canvas
  gyroDeltaY = g.gyro.y * 10;
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Gyro Tracker</title>
  <meta charset="utf-8" />
  <style>
    canvas { border: 1px solid black; background: #f0f0f0; }
    body { font-family: sans-serif; text-align: center; margin-top: 40px; }
  </style>
</head>
<body>
  <h2>Gyroscope Tracker</h2>
  <canvas id="canvas" width="400" height="400"></canvas>
  <p id="status">Waiting for data...</p>
  <script>
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    const status = document.getElementById('status');
    let posX = 200, posY = 200;

    function drawDot(x, y) {
      ctx.clearRect(0, 0, 400, 400);
      ctx.beginPath();
      ctx.arc(x, y, 5, 0, 2 * Math.PI);
      ctx.fillStyle = 'red';
      ctx.fill();
      ctx.stroke();
    }

    async function fetchGyro() {
      try {
        const res = await fetch('/gyro');
        const data = await res.json();
        posX += data.x;
        posY += data.y;

        posX = Math.max(0, Math.min(400, posX));
        posY = Math.max(0, Math.min(400, posY));

        drawDot(posX, posY);

        status.innerText = data.recording ? "Recording ON" : "Recording OFF";
      } catch (e) {
        console.error('Fetch error:', e);
        status.innerText = "Error fetching data";
      }
    }

    setInterval(fetchGyro, 200);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleGyro() {
  updateGyro();
  String json = "{ \"x\": " + String(gyroDeltaX, 2) + 
                ", \"y\": " + String(gyroDeltaY, 2) + 
                ", \"recording\": " + (recording ? "true" : "false") + " }";
  gyroDeltaX = 0;
  gyroDeltaY = 0;
  server.send(200, "application/json", json);
}

void setupButton() {
  pinMode(buttonPin, INPUT_PULLUP);
}

void checkButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      recording = !recording;
      Serial.println(recording ? "Recording STARTED" : "Recording STOPPED");
    }
  }

  lastButtonState = reading;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  setupButton();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected. IP: ");
  Serial.println(WiFi.localIP());

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }

  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  server.on("/", handleRoot);
  server.on("/gyro", handleGyro);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  checkButton();
  server.handleClient();
}
