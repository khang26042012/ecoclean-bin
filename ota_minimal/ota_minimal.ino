/*
 * ECO-CLEAN SENSOR - DÀNH RIÊNG CHO CH N 0 VÀ 1
 * Hàng 2: 5V, GND, 3.3V, 4, 3, 2, 1 (đang cắm), 0 (đang cắm)
 * Tự động khóa đúng chiều Trig/Echo giữa 0 và 1
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long currentDist = -1;
String activeConfig = "Đang dò tìm...";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long pingSensor(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(15); // Xung manh 15us kich hoat cam bien
  digitalWrite(trig, LOW);

  long dur = pulseIn(echo, HIGH, 25000); // 25ms timeout (~4m)
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
    delay(200);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW); // Sang LED on dinh

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("📡 Khoi dong bo do chuyen dung cho cap chan 0 va 1!");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (currentDist >= 0) ? String(currentDist) + " cm" : "Chưa phát hiện vật cản (-1)";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Ultrasonic</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;box-shadow:0 8px 20px rgba(0,0,0,0.5);}";
    html += ".dist{font-size:46px;font-weight:bold;color:#29b6f6;margin:15px 0;}";
    html += ".cfg{color:#ffd54f;font-size:15px;margin-bottom:10px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>CẢM BIẾN SIÊU ÂM</h2>";
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

  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck >= 80) { // Chu ky do chuan 80ms
    lastCheck = now;

    // Kieu A: Trig=0, Echo=1
    long dA = pingSensor(0, 1);
    if (dA >= 2 && dA <= 350) {
      currentDist = dA;
      activeConfig = "✅ Đang nhận: Trig=Chân 0, Echo=Chân 1";
    } else {
      // Kieu B: Trig=1, Echo=0
      long dB = pingSensor(1, 0);
      if (dB >= 2 && dB <= 350) {
        currentDist = dB;
        activeConfig = "✅ Đang nhận: Trig=Chân 1, Echo=Chân 0";
      } else {
        currentDist = -1;
      }
    }

    // Ghi log va chop LED khi phat hien tay (< 30cm)
    if (currentDist >= 0 && currentDist <= 30) {
      digitalWrite(LED_PIN, LOW); // Sang ro khi co tay
      static long lastLogged = -1;
      if (abs(currentDist - lastLogged) >= 2) {
        lastLogged = currentDist;
        addLog("✋ PHÁT HIỆN TAY! Khoảng cách: " + String(currentDist) + " cm");
      }
    } else {
      digitalWrite(LED_PIN, HIGH); // Tat khi khong co tay
    }
  }

  delay(5);
}
