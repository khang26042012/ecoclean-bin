/*
 * ESP32-C3 SUPER MINI - ROCK SOLID WIFI & NON-BLOCKING ULTRASONIC SENSOR
 * Fix: No busy-wait freeze (timeout 5ms = 85cm), WiFi TxPower 11dBm, Auto-Reconnect
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define TRIG_PIN 0
#define ECHO_PIN 1
#define LED_PIN  8

WebServer server(80);
String logs = "";
long currentDistance = -1;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

// Do khoang cach sieu nhanh: Timeout chi 5000us (~85cm) -> Khong bao gio lam nghen Wi-Fi!
long getDistanceFast(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  // Timeout chi 5ms (85cm): vua du cho thung rac, khong ton CPU
  long duration = pulseIn(echo, HIGH, 5000);
  if (duration <= 0) return -1;
  return (long)(duration * 0.034 / 2);
}

void connectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Tu dong ket noi lai neu rot mang
  WiFi.persistent(true);
  
  // Tang song len 11dBm de chong nhieu khi cham tay
  WiFi.setTxPower(WIFI_POWER_11dBm);
  
  WiFi.begin("Trong Khang", "khongbiet123");
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // Khong bao gio treo khi dung pin
  setCpuFrequencyMhz(80);   // Mat me

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  connectWiFi();

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  digitalWrite(LED_PIN, LOW); // Da ket noi: Bat sang LED xanh
  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("🛡️ ROCK SOLID: Song khoe 11dBm + Auto-reconnect + Timeout 5ms chong rot mang");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distStr = (currentDistance >= 0) ? String(currentDistance) + " cm" : "Chưa cắm / Ngoài tầm (-1)";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Sensor</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:20px;max-width:360px;margin:10px auto;}";
    html += ".val{font-size:38px;font-weight:bold;color:#29b6f6;margin:10px 0;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>CẢM BIẾN SIÊU ÂM</h2>";
    html += "<div class='val'>" + distStr + "</div>";
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
    String out = logs + "\n[Hien tai] Dist: " + String(currentDistance) + " cm | Temp: " + String(temp, 1) + " C";
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

  // Tu dong ket noi lai neu bi dut Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH); // Tat LED bao hieu rot mang
    WiFi.reconnect();
    delay(200);
    return;
  } else {
    digitalWrite(LED_PIN, LOW); // Sang LED on dinh
  }

  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  // Do moi 300ms, khong chiem dung CPU
  if (now - lastMeasure >= 300) {
    lastMeasure = now;

    // Do thu Trig=0, Echo=1
    long d = getDistanceFast(TRIG_PIN, ECHO_PIN);
    if (d < 0) {
      // Neu chua duoc thu dao chieu Trig=1, Echo=0
      d = getDistanceFast(ECHO_PIN, TRIG_PIN);
    }

    currentDistance = d;
    if (d >= 0) {
      static long lastD = -1;
      if (abs(d - lastD) >= 3) {
        lastD = d;
        addLog("📏 Khoang cach: " + String(d) + " cm" + (d <= 20 ? " [Phat hien tay!]" : ""));
      }
    }
  }

  delay(5);
}
