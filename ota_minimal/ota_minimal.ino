/*
 * ESP32-C3 SUPER MINI - CẢM BIẾN SIÊU ÂM + PIN STANDALONE FIX
 * Trig: GPIO 0, Echo: GPIO 1, LED: GPIO 8
 * Wi-Fi: "Trong Khang" / "khongbiet123"
 * FIX TRIỆT ĐỂ: Serial.setTxTimeoutMs(0) -> Chạy mượt trên PIN / JACK không bị treo!
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

long getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return -1;
  return (long)(duration * 0.034 / 2);
}

void setup() {
  Serial.begin(115200);

  // 🔥 FIX LỖI TREO KHI DÙNG PIN: Không bao giờ chờ máy tính, cắm pin/jack là chạy ngay!
  Serial.setTxTimeoutMs(0);

  // Ha xung 80MHz tiet kiem pin va mat me
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Tat LED cho ket noi

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Ket noi WiFi nha
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  // Da ket noi: Bat sang LED xanh dung yen
  digitalWrite(LED_PIN, LOW);
  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("🔋 BATTERY FIX OK: Hoat dong hoan hao ca PIN va USB!");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distStr = (currentDistance >= 0) ? String(currentDistance) + " cm" : "Chưa cắm / Ngoài tầm";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Sensor</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:20px;max-width:360px;margin:10px auto;}";
    html += ".val{font-size:36px;font-weight:bold;color:#29b6f6;margin:10px 0;}";
    html += ".temp{color:#66bb6a;font-size:22px;}";
    html += "</style><meta http-equiv='refresh' content='2'></head><body>";
    html += "<div class='card'><h2>CẢM BIẾN SIÊU ÂM</h2>";
    html += "<p>Khoảng cách đo được:</p><div class='val'>" + distStr + "</div>";
    html += "<p class='temp'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffa726;'>Xem Nhật ký (Log) chi tiết</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDistance));
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[Hien tai] Khoang cach: " + String(currentDistance) + " cm | Nhiet do: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do: " + String(temp, 1) + " C");
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
  if (now - lastMeasure >= 500) {
    lastMeasure = now;
    long d = getDistanceCM();
    if (d >= 0 && abs(d - currentDistance) >= 3) {
      addLog("📏 Khoang cach: " + String(d) + " cm" + (d <= 20 ? " [Phat hien tay!]" : ""));
    }
    currentDistance = d;
  }

  delay(10);
}
