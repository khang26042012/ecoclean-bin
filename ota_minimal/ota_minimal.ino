/*
 * ESP32-C3 SUPER MINI - AUTO-DETECT TRIG/ECHO + ULTRASONIC SENSOR
 * Support both normal (Trig=0, Echo=1) and swapped (Trig=1, Echo=0)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long currentDistance = -1;
int trigPin = 0;
int echoPin = 1;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long measureWithPins(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 30000);
  if (duration <= 0) return -1;
  return (long)(duration * 0.034 / 2);
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  digitalWrite(LED_PIN, LOW);
  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("📡 Che do Auto-detect Trig/Echo tren GPIO 0 va 1");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distStr = (currentDistance >= 0) ? String(currentDistance) + " cm" : "Chưa cắm / Ngoài tầm (-1)";
    String pinInfo = "Trig: GPIO " + String(trigPin) + " | Echo: GPIO " + String(echoPin);
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Sensor</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:20px;max-width:360px;margin:10px auto;}";
    html += ".val{font-size:38px;font-weight:bold;color:#29b6f6;margin:10px 0;}";
    html += ".info{color:#aaa;font-size:14px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>CẢM BIẾN SIÊU ÂM</h2>";
    html += "<div class='val'>" + distStr + "</div>";
    html += "<p class='info'>" + pinInfo + "</p>";
    html += "<p style='color:#66bb6a;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffa726;'>Xem Log chi tiết</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDistance));
  });

  server.on("/raw", HTTP_GET, []() {
    int p0 = digitalRead(0);
    int p1 = digitalRead(1);
    server.send(200, "text/plain", "GPIO0=" + String(p0) + " | GPIO1=" + String(p1) + " | Dist=" + String(currentDistance));
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[Hien tai] Dist: " + String(currentDistance) + " cm | Trig=" + String(trigPin) + " Echo=" + String(echoPin) + " | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", "NAP XONG! RESTART...");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  if (now - lastMeasure >= 250) {
    lastMeasure = now;

    // Thu kieu 1: Trig=0, Echo=1
    long d1 = measureWithPins(0, 1);
    if (d1 >= 0 && d1 <= 400) {
      if (trigPin != 0) {
        trigPin = 0; echoPin = 1;
        addLog("✅ Khớp cấu hình: Trig=GPIO 0, Echo=GPIO 1");
      }
      currentDistance = d1;
    } else {
      // Thu kieu 2 (Dao nguoc day): Trig=1, Echo=0
      long d2 = measureWithPins(1, 0);
      if (d2 >= 0 && d2 <= 400) {
        if (trigPin != 1) {
          trigPin = 1; echoPin = 0;
          addLog("✅ Đã tự động đảo dây: Trig=GPIO 1, Echo=GPIO 0");
        }
        currentDistance = d2;
      } else {
        currentDistance = -1;
      }
    }

    if (currentDistance >= 0) {
      static long lastD = -1;
      if (abs(currentDistance - lastD) >= 3) {
        lastD = currentDistance;
        addLog("📏 Khoang cach: " + String(currentDistance) + " cm" + (currentDistance <= 20 ? " [Phat hien tay!]" : ""));
      }
    }
  }

  delay(10);
}
