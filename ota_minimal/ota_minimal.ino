/*
 * ECO-CLEAN BIN - ULTRASONIC SENSOR RUNNER
 * Tests both Trig=0/Echo=1 and Trig=1/Echo=0
 * Instant live distance measurement & Web dashboard
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long currentDist = -1;
long durA = 0;
long durB = 0;
int activeTrig = -1;
int activeEcho = -1;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long ping(int trig, int echo, long &outDur) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(12);
  digitalWrite(trig, LOW);

  long dur = pulseIn(echo, HIGH, 30000);
  outDur = dur;
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
  addLog("📡 Khoi dong bo do khoang cach sieu am tren chan 0 va 1!");

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDist));
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "Dist=" + String(currentDist) + " cm | Trig=" + String(activeTrig) + " Echo=" + String(activeEcho);
    s += " | DurA(0->1)=" + String(durA) + " us | DurB(1->0)=" + String(durB) + " us";
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Khoang cach: " + String(currentDist) + " cm | Trig=" + String(activeTrig) + " Echo=" + String(activeEcho) + " | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (currentDist >= 0) ? String(currentDist) + " cm" : "Đang đo khoảng cách...";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Ultrasonic</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;}";
    html += ".dist{font-size:52px;font-weight:bold;color:" + String(currentDist >= 0 ? "#00e676" : "#ff5252") + ";margin:15px 0;}";
    html += ".info{color:#ffd54f;font-size:16px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>KHOẢNG CÁCH THỰC TẾ</h2>";
    html += "<div class='dist'>" + distText + "</div>";
    html += "<p class='info'>Cấu hình: Trig=" + String(activeTrig) + " | Echo=" + String(activeEcho) + "</p>";
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

  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck >= 80) { // Chu ky do 80ms
    lastCheck = now;

    // Thu chieu 1: Trig=0, Echo=1
    long dA = ping(0, 1, durA);
    if (dA >= 2 && dA <= 350) {
      currentDist = dA;
      activeTrig = 0;
      activeEcho = 1;
    } else {
      delay(20);
      // Thu chieu 2: Trig=1, Echo=0
      long dB = ping(1, 0, durB);
      if (dB >= 2 && dB <= 350) {
        currentDist = dB;
        activeTrig = 1;
        activeEcho = 0;
      } else {
        currentDist = -1;
      }
    }

    if (currentDist >= 0) {
      static long lastLogged = -999;
      if (abs(currentDist - lastLogged) >= 2) {
        lastLogged = currentDist;
        addLog("📏 BẮT ĐƯỢC: " + String(currentDist) + " cm" + (currentDist <= 25 ? " [PHÁT HIỆN TAY!]" : ""));
      }
    }
  }

  delay(5);
}
