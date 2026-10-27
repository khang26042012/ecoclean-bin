/*
 * AGROSENSE PRO: THIẾT BỊ CẦM TAY ĐO ĐỘ ẨM ĐẤT & CHẤT LƯỢNG NƯỚC (BỎ QUA DHT22)
 * 1. Cảm biến Độ Ẩm Đất (Soil Moisture) -> GPIO 0
 * 2. Cảm biến Độ Mặn & Chất Lượng Nước (Water Salinity) -> GPIO 1
 * Giao diện Web Dashboard kép mượt mà, siêu ổn định, chip mát 39°C
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define SOIL_PIN  0 // Cảm biến đất (GPIO 0)
#define WATER_PIN 1 // Cảm biến độ mặn nước (GPIO 1)
#define LED_PIN   8

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
String waterAdvice = "Nhúng đầu que vào nước để đo độ mặn";
String waterColor = "#888";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

void readSensors() {
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
  addLog("🌱 KHOI DONG TRẠM ĐO: Do am dat (GPIO 0) + Do man nuoc (GPIO 1)!");

  // Giao diện Web Dashboard kép siêu đẹp
  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String soilColor = (soilPercent < 30) ? "#ff5252" : (soilPercent <= 75 ? "#00e676" : "#29b6f6");
    
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>AgroSense Pro - Máy Đo Cầm Tay</title>";
    html += "<style>";
    html += "body{font-family:sans-serif;text-align:center;padding:15px;background:#121212;color:#fff;}";
    html += ".header{margin-bottom:15px;}";
    html += ".card{background:#1e1e1e;border-radius:16px;padding:20px;max-width:400px;margin:12px auto;box-shadow:0 8px 24px rgba(0,0,0,0.6);text-align:left;}";
    html += ".title{font-size:14px;color:#aaa;text-transform:uppercase;letter-spacing:1px;margin-bottom:5px;}";
    html += ".val{font-size:46px;font-weight:bold;margin:8px 0;}";
    html += ".badge{display:inline-block;padding:6px 12px;border-radius:6px;font-size:14px;font-weight:bold;margin-top:5px;}";
    html += ".desc{font-size:13px;color:#bbb;margin-top:10px;line-height:1.4;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    
    html += "<div class='header'><h2 style='margin:0;color:#00e676;'>🌱 AGROSENSE PRO</h2>";
    html += "<p style='margin:4px 0;color:#888;font-size:13px;'>BỘ QUAN TRẮC ĐỘ ẨM ĐẤT & CẢNH BÁO XÂM NHẬP MẶN</p></div>";

    // Khối 1: Độ ẩm đất
    html += "<div class='card'>";
    html += "<div class='title'>1. Độ Ẩm Đất Trồng (Soil Moisture - GPIO 0)</div>";
    html += "<div class='val' style='color:" + soilColor + ";'>" + String(soilPercent) + "%</div>";
    html += "<div class='badge' style='background:#2a2a2a;color:" + soilColor + ";'>" + soilStatus + "</div>";
    html += "<div class='desc'>Tín hiệu ADC: " + String(rawSoil) + " / 4095</div>";
    html += "</div>";

    // Khối 2: Chất lượng & Độ mặn nước
    html += "<div class='card'>";
    html += "<div class='title'>2. Chất Lượng & Độ Mặn Nước Tưới (Salinity - GPIO 1)</div>";
    html += "<div class='val' style='color:" + waterColor + ";'>" + (rawWater < 200 ? "CHƯA NHÚNG" : String(salinityEst, 1) + " &permil;") + "</div>";
    html += "<div class='badge' style='background:#2a2a2a;color:" + waterColor + ";'>" + waterQuality + "</div>";
    html += "<div class='desc'>" + waterAdvice + "</div>";
    html += "<div class='desc' style='color:#777;'>Độ dẫn điện ADC: " + String(rawWater) + " / 4095</div>";
    html += "</div>";

    html += "<p style='color:#81c784;font-size:14px;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C | <a href='/log' style='color:#ffa726;text-decoration:none;'>Xem Nhật ký (Log)</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/water", HTTP_GET, []() {
    server.send(200, "text/plain", "RawADC=" + String(rawWater) + " | Salinity=" + String(salinityEst, 1) + " permil | Quality=" + waterQuality);
  });

  server.on("/soil", HTTP_GET, []() {
    server.send(200, "text/plain", String(soilPercent));
  });

  server.on("/raw", HTTP_GET, []() {
    server.send(200, "text/plain", "SoilADC=" + String(rawSoil) + " (" + String(soilPercent) + "%) | WaterADC=" + String(rawWater) + " (" + String(salinityEst, 1) + " permil)");
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Dat: " + String(soilPercent) + "% | Nuoc: " + String(salinityEst, 1) + " permil (ADC " + String(rawWater) + ") | Temp: " + String(temp, 1) + " C";
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
  if (now - lastMeasure >= 250) {
    lastMeasure = now;
    readSensors();

    static int lastWaterLogged = -999;
    if (abs(rawWater - lastWaterLogged) >= 200) {
      lastWaterLogged = rawWater;
      addLog("🌊 Nguồn nước thay đổi (ADC " + String(rawWater) + "): " + waterQuality + " ~ " + String(salinityEst, 1) + " ‰");
    }
  }

  delay(5);
}
