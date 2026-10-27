/*
 * AGROSENSE PRO 3-IN-1: HỆ THỐNG QUAN TRẮC NÔNG NGHIỆP CẦM TAY TOÀN DIỆN
 * 1. Độ ẩm đất (Soil Moisture) -> GPIO 0
 * 2. Chất lượng & Độ mặn nước (Water Salinity) -> GPIO 1
 * 3. Nhiệt độ & Độ ẩm không khí (DHT22) -> GPIO 2
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define SOIL_PIN   0 // Cảm biến đất
#define WATER_PIN  1 // Cảm biến độ mặn nước
#define DHT_PIN    2 // Cảm biến DHT22 (Chân out)
#define LED_PIN    8

WebServer server(80);
String logs = "";

// Biến cảm biến đất
int rawSoil = 0;
int soilPercent = 0;
String soilStatus = "Đang đo...";

// Biến cảm biến nước
int rawWater = 0;
float salinityEst = 0.0;
String waterQuality = "Chưa nhúng nước";
String waterAdvice = "Nhúng đầu que vào nước để kiểm tra";
String waterColor = "#888";

// Biến cảm biến DHT22
float dhtTemp = -999.0;
float dhtHum = -999.0;
bool dhtValid = false;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 3000) logs = logs.substring(logs.indexOf('\n') + 1);
}

// Hàm đọc dữ liệu DHT22 chuẩn giao thức 1-wire
bool readDHT22(int pin, float &t_out, float &h_out) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(1200);
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  pinMode(pin, INPUT_PULLUP);

  unsigned long timeout = micros();
  while (digitalRead(pin) == HIGH) { if (micros() - timeout > 90) return false; }
  timeout = micros();
  while (digitalRead(pin) == LOW)  { if (micros() - timeout > 90) return false; }
  timeout = micros();
  while (digitalRead(pin) == HIGH) { if (micros() - timeout > 90) return false; }

  for (int i = 0; i < 40; i++) {
    timeout = micros();
    while (digitalRead(pin) == LOW) { if (micros() - timeout > 90) return false; }
    unsigned long t = micros();
    while (digitalRead(pin) == HIGH) { if (micros() - timeout > 120) return false; }
    if ((micros() - t) > 40) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return false;

  h_out = ((data[0] << 8) | data[1]) * 0.1f;
  int16_t rawT = ((data[2] & 0x7F) << 8) | data[3];
  if (data[2] & 0x80) rawT = -rawT;
  t_out = rawT * 0.1f;
  return true;
}

void readAllSensors() {
  // 1. ĐO ĐỘ ẨM ĐẤT (GPIO 0)
  int sVal = analogRead(SOIL_PIN);
  rawSoil = sVal;
  int sPct = map(sVal, 3200, 1100, 0, 100);
  soilPercent = constrain(sPct, 0, 100);
  if (soilPercent < 30) {
    soilStatus = "🌵 ĐẤT KHÔ (Cần tưới ngay)";
  } else if (soilPercent <= 75) {
    soilStatus = "🌱 ĐỘ ẨM LÝ TƯỞNG (Cây tốt)";
  } else {
    soilStatus = "💧 ĐẤT RẤT ẨM / NGẬP NƯỚC";
  }

  // 2. ĐO ĐỘ MẶN & CHẤT LƯỢNG NƯỚC (GPIO 1)
  int wVal = analogRead(WATER_PIN);
  rawWater = wVal;
  if (wVal < 200) {
    waterQuality = "KHÔ RÁO (Chưa nhúng nước)";
    salinityEst = 0.0;
    waterAdvice = "Nhúng đầu que vào ly/kênh nước để đo độ mặn";
    waterColor = "#888888";
  } else if (wVal < 1000) {
    waterQuality = "💧 NƯỚC NGỌT TINH KHIẾT";
    salinityEst = 0.1;
    waterAdvice = "Nước mưa / nước lọc sạch, không có tạp chất";
    waterColor = "#29b6f6";
  } else if (wVal < 2100) {
    waterQuality = "🌱 NƯỚC NGỌT TƯỚI TIÊU AN TOÀN";
    salinityEst = 0.4;
    waterAdvice = "Nước ngọt đạt chuẩn tưới sầu riêng, hoa màu";
    waterColor = "#00e676";
  } else if (wVal < 3100) {
    waterQuality = "⚠️ CẢNH BÁO: NƯỚC LỢ / NHIỄM MẶN";
    salinityEst = 1.5;
    waterAdvice = "Độ mặn 1 - 2‰! Tuyệt đối không tưới sầu riêng!";
    waterColor = "#ffa726";
  } else {
    waterQuality = "☠️ NGUY HIỂM: NƯỚC NHIỄM MẶN NẶNG";
    salinityEst = 4.5;
    waterAdvice = "Độ mặn > 4‰! CẤM BƠM VÀO VƯỜN CÂY ĂN TRÁI!";
    waterColor = "#ff5252";
  }

  // 3. ĐO NHIỆT ĐỘ & ĐỘ ẨM KHÔNG KHÍ (DHT22 - GPIO 2)
  float t = 0, h = 0;
  if (readDHT22(DHT_PIN, t, h)) {
    dhtTemp = t;
    dhtHum = h;
    dhtValid = true;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80);

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
  digitalWrite(LED_PIN, LOW);

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("🌱 KÍCH HOẠT FULL BỘ 3 CẢM BIẾN: Đất (GPIO 0) + Nước (GPIO 1) + DHT22 (GPIO 2)!");

  server.on("/", HTTP_GET, []() {
    float chipTemp = temperatureRead();
    String soilColor = (soilPercent < 30) ? "#ff5252" : (soilPercent <= 75 ? "#00e676" : "#29b6f6");
    String dhtTStr = dhtValid ? (String(dhtTemp, 1) + " &deg;C") : "Đang chờ cắm...";
    String dhtHStr = dhtValid ? (String(dhtHum, 1) + "%") : "Đang chờ cắm...";

    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>AgroSense Pro - Thiết Bị Cầm Tay</title>";
    html += "<style>";
    html += "body{font-family:sans-serif;text-align:center;padding:12px;background:#121212;color:#fff;}";
    html += ".header{margin-bottom:12px;}";
    html += ".grid{display:flex;flex-wrap:wrap;justify-content:center;gap:12px;max-width:440px;margin:0 auto;}";
    html += ".card{background:#1e1e1e;border-radius:14px;padding:15px;box-shadow:0 6px 20px rgba(0,0,0,0.5);text-align:left;flex:1 1 180px;}";
    html += ".fullcard{background:#1e1e1e;border-radius:14px;padding:16px;box-shadow:0 6px 20px rgba(0,0,0,0.5);text-align:left;width:100%;max-width:440px;margin:12px auto;}";
    html += ".title{font-size:12px;color:#aaa;text-transform:uppercase;letter-spacing:1px;}";
    html += ".val{font-size:38px;font-weight:bold;margin:6px 0;}";
    html += ".badge{display:inline-block;padding:5px 10px;border-radius:6px;font-size:13px;font-weight:bold;}";
    html += ".desc{font-size:12px;color:#bbb;margin-top:8px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";

    html += "<div class='header'><h2 style='margin:0;color:#00e676;'>🌱 AGROSENSE PRO</h2>";
    html += "<p style='margin:4px 0;color:#888;font-size:12px;'>TRẠM QUAN TRẮC ĐỘ ẨM ĐẤT, NƯỚC TƯỚI & VI KHÍ HẬU</p></div>";

    // Khối 1: Đất
    html += "<div class='fullcard'>";
    html += "<div class='title'>1. Độ Ẩm Đất Trồng (Soil Moisture - GPIO 0)</div>";
    html += "<div class='val' style='color:" + soilColor + ";'>" + String(soilPercent) + "%</div>";
    html += "<div class='badge' style='background:#2a2a2a;color:" + soilColor + ";'>" + soilStatus + "</div>";
    html += "</div>";

    // Khối 2: Nước
    html += "<div class='fullcard'>";
    html += "<div class='title'>2. Độ Mặn & Chất Lượng Nước Tưới (Salinity - GPIO 1)</div>";
    html += "<div class='val' style='color:" + waterColor + ";'>" + (rawWater < 200 ? "KHÔ RÁO" : String(salinityEst, 1) + " &permil;") + "</div>";
    html += "<div class='badge' style='background:#2a2a2a;color:" + waterColor + ";'>" + waterQuality + "</div>";
    html += "<div class='desc'>" + waterAdvice + "</div>";
    html += "</div>";

    // Khối 3 & 4: DHT22
    html += "<div class='grid'>";
    html += "<div class='card'>";
    html += "<div class='title'>3. Nhiệt Độ Không Khí</div>";
    html += "<div class='val' style='color:#ffb74d;'>" + dhtTStr + "</div>";
    html += "<div class='desc'>DHT22 - Chân 2</div>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<div class='title'>4. Độ Ẩm Không Khí</div>";
    html += "<div class='val' style='color:#29b6f6;'>" + dhtHStr + "</div>";
    html += "<div class='desc'>DHT22 - Chân 2</div>";
    html += "</div>";
    html += "</div>";

    html += "<p style='color:#81c784;font-size:13px;margin-top:15px;'>Nhiệt độ chip ESP32: " + String(chipTemp, 1) + " &deg;C | <a href='/log' style='color:#ffa726;text-decoration:none;'>Xem Nhật Ký (Log)</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/data", HTTP_GET, []() {
    String out = "Soil=" + String(soilPercent) + "% | WaterSalinity=" + String(salinityEst, 1) + " permil | AirTemp=" + String(dhtTemp, 1) + " C | AirHum=" + String(dhtHum, 1) + "%";
    server.send(200, "text/plain", out);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Dat: " + String(soilPercent) + "% | Nuoc: " + String(salinityEst, 1) + " permil | DHT22: " + String(dhtTemp, 1) + "C, " + String(dhtHum, 1) + "% | Temp: " + String(temp, 1) + " C";
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
  if (now - lastMeasure >= 1000) {
    lastMeasure = now;
    readAllSensors();

    static float lastLoggedT = -999;
    if (dhtValid && abs(dhtTemp - lastLoggedT) >= 0.5) {
      lastLoggedT = dhtTemp;
      addLog("🌡️ Khí hậu: " + String(dhtTemp, 1) + " °C | Độ ẩm KK: " + String(dhtHum, 1) + "%");
    }
  }

  delay(5);
}
