/*
 * ECO-CLEAN SENSOR - REAL-TIME TRANSPARENT SENSOR READINGS
 * Trig: GPIO 20, Echo: GPIO 21
 * Always reports real measurement (including -1 if out of range)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define TRIG_PIN 20
#define ECHO_PIN 21
#define LED_PIN  8

WebServer server(80);
String logs = "";
long currentDist = -1;
long rawDuration = 0;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long readUltrasonic() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Dam bao Trig o muc LOW truoc
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);

  // Phat xung 10us
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn timeout 25000us (~4.2m)
  long dur = pulseIn(ECHO_PIN, HIGH, 25000);
  rawDuration = dur;

  if (dur <= 0) return -1;
  return (long)(dur * 0.034 / 2);
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(150);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW); // Sang LED on dinh

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("📡 Che do do truc tiep Trig=20, Echo=21 (Khong loc so cu)");

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDist));
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "Dist=" + String(currentDist) + " cm | Duration=" + String(rawDuration) + " us | EchoPinState=" + String(digitalRead(ECHO_PIN));
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Dist: " + String(currentDist) + " cm | RawDur: " + String(rawDuration) + " us | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (currentDist >= 0) ? String(currentDist) + " cm" : "Ngoài tầm / Timeout (-1)";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Live Distance</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;}";
    html += ".dist{font-size:52px;font-weight:bold;color:" + String(currentDist >= 0 ? "#00e676" : "#ff5252") + ";margin:15px 0;}";
    html += ".info{color:#ffd54f;font-size:16px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>KHOẢNG CÁCH THỰC TẾ</h2>";
    html += "<div class='dist'>" + distText + "</div>";
    html += "<p class='info'>Thời gian xung: " + String(rawDuration) + " &micro;s</p>";
    html += "<p style='color:#81c784;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffb74d;'>Xem Nhật ký (Log)</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
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

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  static unsigned long lastPing = 0;
  unsigned long now = millis();
  if (now - lastPing >= 80) { // Chu ky do 80ms
    lastPing = now;
    
    // Luon cap nhat gia tri that, khong giu lai so cu
    long d = readUltrasonic();
    currentDist = d;

    // Ghi log khi co thay doi
    static long lastLogged = -999;
    if (abs(d - lastLogged) >= 2) {
      lastLogged = d;
      if (d >= 0) {
        addLog("📏 Khoang cach: " + String(d) + " cm (" + String(rawDuration) + " us)" + (d <= 25 ? " [PHAT HIEN TAY!]" : ""));
      } else {
        addLog("⚠️ Ngoai tam do (-1)");
      }
    }
  }

  delay(5);
}
