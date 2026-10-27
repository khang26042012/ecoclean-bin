/*
 * ECO-CLEAN SENSOR - CANONICAL ARDUINO CODE (NO FLIP-FLOPPING)
 * Mode A: Trig=GPIO 0 (OUTPUT), Echo=GPIO 1 (INPUT)
 * Mode B: Trig=GPIO 1 (OUTPUT), Echo=GPIO 0 (INPUT)
 * Selectable via Web API, pinModes are strictly permanent!
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
int mode = 1; // 1 = Mode A (Trig 0, Echo 1), 2 = Mode B (Trig 1, Echo 0)
long currentDist = -1;
long rawDuration = 0;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

void applyPinMode() {
  if (mode == 1) {
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);
    pinMode(1, INPUT);
    addLog("🔧 Đã chuyển sang Mode A: Trig=GPIO 0, Echo=GPIO 1");
  } else {
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    pinMode(0, INPUT);
    addLog("🔧 Đã chuyển sang Mode B: Trig=GPIO 1, Echo=GPIO 0");
  }
}

long measureCanonical() {
  int trig = (mode == 1) ? 0 : 1;
  int echo = (mode == 1) ? 1 : 0;

  // Xung chuan theo datasheet HC-SR04
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  // pulseIn chuan timeout 30ms (~5m)
  long dur = pulseIn(echo, HIGH, 30000);
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

  // Ap dung che do khoi dau
  applyPinMode();

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDist));
  });

  server.on("/mode1", HTTP_GET, []() {
    mode = 1;
    applyPinMode();
    server.send(200, "text/plain", "OK: Mode A (Trig=0, Echo=1)");
  });

  server.on("/mode2", HTTP_GET, []() {
    mode = 2;
    applyPinMode();
    server.send(200, "text/plain", "OK: Mode B (Trig=1, Echo=0)");
  });

  server.on("/raw", HTTP_GET, []() {
    int p0 = digitalRead(0);
    int p1 = digitalRead(1);
    String s = "Mode=" + String(mode == 1 ? "A(Trig0,Echo1)" : "B(Trig1,Echo0)") + " | Dist=" + String(currentDist) + " cm | Dur=" + String(rawDuration) + " us | P0=" + String(p0) + " P1=" + String(p1);
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Mode: " + String(mode == 1 ? "A" : "B") + " | Dist: " + String(currentDist) + " cm | RawDur: " + String(rawDuration) + " us | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (currentDist >= 0) ? String(currentDist) + " cm" : "Ngoài tầm (-1)";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Canonical</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;}";
    html += ".dist{font-size:52px;font-weight:bold;color:" + String(currentDist >= 0 ? "#00e676" : "#ff5252") + ";margin:15px 0;}";
    html += ".btn{display:inline-block;padding:10px 20px;margin:5px;background:#29b6f6;color:#000;text-decoration:none;border-radius:8px;font-weight:bold;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>CANONICAL SENSOR TEST</h2>";
    html += "<div class='dist'>" + distText + "</div>";
    html += "<p>Chế độ hiện tại: <b>Mode " + String(mode == 1 ? "A (Trig 0, Echo 1)" : "B (Trig 1, Echo 0)") + "</b></p>";
    html += "<p><a class='btn' href='/mode1'>Thử Mode A (0-1)</a> <a class='btn' href='/mode2'>Thử Mode B (1-0)</a></p>";
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
  if (now - lastPing >= 100) { // Chu ky 100ms
    lastPing = now;
    
    long d = measureCanonical();
    currentDist = d;

    static long lastLogged = -999;
    if (abs(d - lastLogged) >= 2) {
      lastLogged = d;
      if (d >= 0) {
        addLog("📏 DO DUOC: " + String(d) + " cm (" + String(rawDuration) + " us)");
      }
    }
  }

  delay(5);
}
