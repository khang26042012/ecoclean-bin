/*
 * ECO-CLEAN BIN - FORCED ECHO UNLOCK & HIGH-VOLTAGE HYBRID TRIGGER
 * Algorithm: Hardware state discharge before every ping to break clone HC-SR04 lock-up
 * Pins: GPIO 0 & GPIO 1
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long currentDist = -1;
String activeConfig = "Đang mở khóa cảm biến...";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

// Ham do khoang cach voi thuat toan "Cuong che xa khoa Echo"
long measureWithForcedUnlock(int trig, int echo) {
  // 1. CƯỠNG CHẾ XẢ ĐIỆN TÍCH KẸT: Keo ca 2 chan ve LOW de reset vi dieu khien cam bien
  pinMode(echo, OUTPUT);
  digitalWrite(echo, LOW);
  pinMode(trig, OUTPUT);
  digitalWrite(trig, LOW);
  delayMicroseconds(50); // Xa trong 50us

  // 2. Chuyen Echo sang che do INPUT san sang nhan tin hieu
  pinMode(echo, INPUT_PULLDOWN);

  // 3. Phat xung kich Trig manh 20us (vuot nguong 3.5V cua clone HC-SR04)
  digitalWrite(trig, HIGH);
  delayMicroseconds(20);
  digitalWrite(trig, LOW);

  // 4. Do do rong xung Echo (timeout 30ms ~ 5m)
  long dur = pulseIn(echo, HIGH, 30000);
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
  addLog("🔓 Kich hoat thuat toan CUONG CHE XA KHOA ECHO tren GPIO 0 & 1!");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (currentDist >= 0) ? String(currentDist) + " cm" : "Chưa đo được (-1)";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Unlock</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;}";
    html += ".dist{font-size:48px;font-weight:bold;color:#00e676;margin:15px 0;}";
    html += ".cfg{color:#ffd54f;font-size:15px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>CẢM BIẾN ĐÃ MỞ KHÓA</h2>";
    html += "<div class='dist'>" + distText + "</div>";
    html += "<p class='cfg'>" + activeConfig + "</p>";
    html += "<p style='color:#81c784;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffb74d;'>Xem Nhật ký (Log)</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDist));
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "GPIO0=" + String(digitalRead(0)) + " | GPIO1=" + String(digitalRead(1)) + " | Dist=" + String(currentDist) + " cm";
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[Hien tai] Dist: " + String(currentDist) + " cm | " + activeConfig + " | Temp: " + String(temp, 1) + " C";
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

  // Luon giu LED sang on dinh khi co WiFi
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  }

  static unsigned long lastPing = 0;
  unsigned long now = millis();
  // Chu ky 100ms de cam bien co du thoi gian nghi giua cac lan phat
  if (now - lastPing >= 100) {
    lastPing = now;

    // Chieu A: Trig=0, Echo=1
    long dA = measureWithForcedUnlock(0, 1);
    if (dA >= 2 && dA <= 350) {
      currentDist = dA;
      activeConfig = "✅ Đang nhận: Trig=Chân 0, Echo=Chân 1";
    } else {
      // Chieu B: Trig=1, Echo=0
      long dB = measureWithForcedUnlock(1, 0);
      if (dB >= 2 && dB <= 350) {
        currentDist = dB;
        activeConfig = "✅ Đang nhận: Trig=Chân 1, Echo=Chân 0";
      } else {
        currentDist = -1;
      }
    }

    if (currentDist >= 0) {
      static long lastD = -1;
      if (abs(currentDist - lastD) >= 2) {
        lastD = currentDist;
        addLog("📏 Khoang cach do duoc: " + String(currentDist) + " cm" + (currentDist <= 25 ? " [Co vat can gan!]" : ""));
      }
    }
  }

  delay(5);
}
