/*
 * AGROSENSE MINI - THIẾT BỊ CẦM TAY ĐO ĐỘ ẨM ĐẤT
 * Cảm biến loại 1 (LM393): VCC -> 3V3, GND -> GND, A0 -> GPIO 0
 * Hiển thị Web Dashboard siêu đẹp theo thời gian thực
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define SOIL_PIN 0 // Chân Analog đọc độ ẩm đất
#define LED_PIN  8

WebServer server(80);
String logs = "";
int rawSoil = 0;
int soilPercent = 0;
String soilStatus = "Đang đo...";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

void readSoilSensor() {
  // Đọc giá trị ADC 12-bit (0 - 4095)
  int val = analogRead(SOIL_PIN);
  rawSoil = val;

  // Cân chỉnh thực tế:
  // Để ngoài không khí khô: val ~ 3200 - 3600 (0%)
  // Cắm vào nước / đất ẩm: val ~ 1000 - 1400 (100%)
  int pct = map(val, 3200, 1200, 0, 100);
  pct = constrain(pct, 0, 100);
  soilPercent = pct;

  if (pct < 30) {
    soilStatus = "🌵 ĐẤT KHÔ HẠN (Cần tưới nước ngay!)";
  } else if (pct <= 75) {
    soilStatus = "🌱 ĐỘ ẨM LÝ TƯỞNG (Cây phát triển tốt)";
  } else {
    soilStatus = "💧 ĐẤT RẤT ẨM / NGẬP NƯỚC (Ngừng tưới)";
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80); // Mát mẻ 38-40C

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(150);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW); // Đèn xanh sáng đứng yên

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("🌱 AgroSense Mini da san sang! Doc cam bien tren GPIO 0");

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String color = (soilPercent < 30) ? "#ff5252" : (soilPercent <= 75 ? "#00e676" : "#29b6f6");
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>AgroSense Mini</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#141414;color:#fff;}";
    html += ".card{background:#1f1f1f;border-radius:18px;padding:25px;max-width:380px;margin:10px auto;box-shadow:0 8px 24px rgba(0,0,0,0.6);}";
    html += ".val{font-size:56px;font-weight:bold;color:" + color + ";margin:10px 0;}";
    html += ".status{font-size:16px;font-weight:bold;padding:10px;border-radius:8px;background:#2a2a2a;margin:15px 0;}";
    html += ".raw{color:#888;font-size:13px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'>";
    html += "<h2 style='margin-top:0;'>🌱 AGROSENSE MINI</h2>";
    html += "<p style='color:#bbb;'>ĐỘ ẨM ĐẤT TRỒNG TRỌT</p>";
    html += "<div class='val'>" + String(soilPercent) + "%</div>";
    html += "<div class='status'>" + soilStatus + "</div>";
    html += "<p class='raw'>Điện áp ADC: " + String(rawSoil) + " / 4095</p>";
    html += "<p style='color:#66bb6a;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffa726;text-decoration:none;'>Xem Nhật ký (Log)</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/soil", HTTP_GET, []() {
    server.send(200, "text/plain", String(soilPercent));
  });

  server.on("/raw", HTTP_GET, []() {
    server.send(200, "text/plain", "RawADC=" + String(rawSoil) + " | Percent=" + String(soilPercent) + "%");
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Do am dat: " + String(soilPercent) + "% (ADC: " + String(rawSoil) + ") | Temp: " + String(temp, 1) + " C";
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

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  if (now - lastMeasure >= 300) {
    lastMeasure = now;
    readSoilSensor();

    static int lastLoggedPct = -999;
    if (abs(soilPercent - lastLoggedPct) >= 5) {
      lastLoggedPct = soilPercent;
      addLog("🌱 Độ ẩm đất thay đổi: " + String(soilPercent) + "% (ADC: " + String(rawSoil) + ") -> " + soilStatus);
    }
  }

  delay(5);
}
