/*
 * AGROSENSE PRO - DUAL-NETWORK + STANDALONE AP + PRECISION MULTI-SENSOR
 * Hardware: ESP32-C3 Super Mini
 * Sensor 1 (Soil Moisture): GPIO 0
 * Sensor 2 (Water Salinity & Quality): GPIO 1
 * Status LED: GPIO 8 (LOW = ON)
 * Dual WiFi Station: "Trong Khang" & "phantrongkhangg"
 * Broadcast SoftAP: "Esp32c3 - hệ thống modlun"
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define SOIL_PIN  0 // GPIO 0 (ADC1_CH0)
#define WATER_PIN 1 // GPIO 1 (ADC1_CH1)
#define LED_PIN   8 // LED xanh tich hop

WebServer server(80);

// Thong tin mang
const char* AP_SSID = "Esp32c3 - hệ thống modlun";

struct WiFiCred {
  const char* ssid;
  const char* pass;
};

const WiFiCred NETWORKS[] = {
  {"Trong Khang", "khongbiet123"},
  {"phantrongkhangg", "26042012khang"}
};
const int NUM_NETWORKS = sizeof(NETWORKS) / sizeof(NETWORKS[0]);

String connectedSSID = "Không có (Đang chạy AP độc lập)";
String localIPStr = "192.168.4.1";
bool isStationConnected = false;
String logs = "";

// Du lieu cam bien da qua bo loc
int soilADC = 0;
int soilPercent = 0;
String soilStatus = "Đang đo...";

int waterADC = 0;
float waterSalinity = 0.0f;
String waterQuality = "Chưa nhúng";
String waterAdvice = "Nhúng đầu dò vào nước để kiểm tra";
String waterColor = "#888888";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 3000) logs = logs.substring(logs.indexOf('\n') + 1);
}

// BỘ LỌC CHÍNH XÁC CAO: Đọc 25 mẫu, sắp xếp loại bỏ nhiễu đỉnh/đáy, lấy trung bình
int readPrecisionADC(int pin) {
  const int SAMPLES = 25;
  int raw[SAMPLES];
  for (int i = 0; i < SAMPLES; i++) {
    raw[i] = analogRead(pin);
    delayMicroseconds(200);
  }
  // Sắp xếp nổi bọt (Bubble sort)
  for (int i = 0; i < SAMPLES - 1; i++) {
    for (int j = i + 1; j < SAMPLES; j++) {
      if (raw[i] > raw[j]) {
        int temp = raw[i]; raw[i] = raw[j]; raw[j] = temp;
      }
    }
  }
  // Bỏ 5 mẫu thấp nhất và 5 mẫu cao nhất (outliers), lấy trung bình 15 mẫu ở giữa
  long sum = 0;
  for (int i = 5; i < 20; i++) {
    sum += raw[i];
  }
  return (int)(sum / 15);
}

// THUẬT TOÁN ĐO CHUẨN XÁC ĐỘ ẨM ĐẤT
void computeSoilMoisture(int adc) {
  soilADC = adc;
  // Cân chỉnh: 3600 (khô ngoài không khí 0%) đến 1150 (ngập nước 100%)
  int pct = map(adc, 3600, 1150, 0, 100);
  pct = constrain(pct, 0, 100);
  soilPercent = pct;

  if (pct < 25) {
    soilStatus = "🌵 ĐẤT KHÔ HẠN (Cần tưới ngay)";
  } else if (pct <= 75) {
    soilStatus = "🌱 ĐỘ ẨM LÝ TƯỞNG (Cây tốt)";
  } else {
    soilStatus = "💧 ĐẤT RẤT ẨM / NGẬP NƯỚC (Ngừng tưới)";
  }
}

// THUẬT TOÁN ĐO CHUẨN XÁC CHẤT LƯỢNG & ĐỘ MẶN NƯỚC TƯỚI
void computeWaterSalinity(int adc) {
  waterADC = adc;
  
  if (adc < 120) {
    // Que khô ráo, chưa chạm nước
    waterSalinity = 0.0f;
    waterQuality = "KHÔ RÁO (Chưa nhúng nước)";
    waterAdvice = "Hãy nhúng que cảm biến vào xô/kênh nước để đo độ mặn.";
    waterColor = "#757575";
  } else if (adc < 850) {
    // Nước mưa, nước cất, nước lọc tinh khiết
    waterSalinity = (adc - 120) * 0.2f / 730.0f; // 0.0 - 0.2 ‰
    waterQuality = "💧 NƯỚC NGỌT TINH KHIẾT (Nước sạch)";
    waterAdvice = "Nước rất sạch, nồng độ khoáng/muối cực thấp. Rất an toàn.";
    waterColor = "#29b6f6";
  } else if (adc < 1850) {
    // Nước ngọt tưới tiêu sông ngòi chuẩn nông nghiệp (Bến Tre / ĐBSCL)
    waterSalinity = 0.2f + (adc - 850) * 0.6f / 1000.0f; // 0.2 - 0.8 ‰
    waterQuality = "🌱 NƯỚC NGỌT TƯỚI AN TOÀN";
    waterAdvice = "Độ mặn an toàn (< 0.8‰). Bơm tưới thoải mái cho sầu riêng, chôm chôm, hoa kiểng.";
    waterColor = "#00e676";
  } else if (adc < 2900) {
    // Nước chớm lợ / nhiễm mặn mức độ 1-2‰
    waterSalinity = 0.8f + (adc - 1850) * 1.5f / 1050.0f; // 0.8 - 2.3 ‰
    waterQuality = "⚠️ CẢNH BÁO: NƯỚC LỢ / NHIỄM MẶN";
    waterAdvice = "Nước có dấu hiệu nhiễm mặn! CẤM tưới cho cây sầu riêng, măng cụt, bưởi.";
    waterColor = "#ffa726";
  } else {
    // Nước nhiễm mặn nặng / nước muối
    waterSalinity = 2.3f + (adc - 2900) * 3.5f / 1195.0f; // 2.3 - 5.8+ ‰
    waterQuality = "☠️ NGUY HIỂM: NƯỚC NHIỄM MẶN NẶNG";
    waterAdvice = "ĐỘ MẶN QUÁ CAO (> 2.5‰)! Tuyệt đối KHÔNG được bơm vào mương vườn!";
    waterColor = "#ff5252";
  }
}

// Giao diện Web Dashboard Hiện Đại - Responsive (HTML/CSS/JS)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AgroSense Pro - Trạm Quan Trắc Cầm Tay</title>
  <style>
    :root {
      --bg: #0f141c;
      --card-bg: rgba(26, 34, 48, 0.85);
      --border: rgba(255, 255, 255, 0.08);
      --primary: #00e676;
      --text: #ffffff;
      --text-dim: #90a4ae;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .container { width: 100%; max-width: 480px; }
    header { text-align: center; margin-bottom: 16px; }
    .badge-top {
      display: inline-block;
      background: rgba(0, 230, 118, 0.15);
      color: var(--primary);
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 1px;
      text-transform: uppercase;
      margin-bottom: 8px;
      border: 1px solid rgba(0, 230, 118, 0.3);
    }
    h1 { font-size: 22px; font-weight: 800; letter-spacing: -0.5px; }
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 20px;
      margin-bottom: 14px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
      transition: transform 0.2s ease;
    }
    .card-title {
      font-size: 12px;
      font-weight: 700;
      color: var(--text-dim);
      text-transform: uppercase;
      letter-spacing: 1px;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .metric-box { display: flex; align-items: baseline; justify-content: space-between; margin: 12px 0 8px 0; }
    .metric-val { font-size: 48px; font-weight: 800; line-height: 1; }
    .metric-unit { font-size: 20px; font-weight: 600; color: var(--text-dim); margin-left: 4px; }
    .bar-bg {
      width: 100%;
      height: 8px;
      background: rgba(255, 255, 255, 0.08);
      border-radius: 4px;
      overflow: hidden;
      margin-bottom: 12px;
    }
    .bar-fill { height: 100%; width: 0%; border-radius: 4px; transition: width 0.4s ease, background-color 0.4s ease; }
    .status-badge {
      display: inline-block;
      font-size: 13px;
      font-weight: 700;
      padding: 6px 12px;
      border-radius: 8px;
      background: rgba(255, 255, 255, 0.05);
      margin-bottom: 8px;
    }
    .advice-text { font-size: 13px; line-height: 1.45; color: #cfd8dc; }
    .raw-info { font-size: 11px; color: var(--text-dim); margin-top: 10px; border-top: 1px solid var(--border); padding-top: 8px; }
    .sys-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 4px; }
    .sys-item { background: rgba(255, 255, 255, 0.03); padding: 10px; border-radius: 10px; font-size: 12px; }
    .sys-label { color: var(--text-dim); margin-bottom: 2px; }
    .sys-value { font-weight: 700; color: #fff; word-break: break-all; }
    footer { text-align: center; margin-top: 10px; font-size: 12px; color: var(--text-dim); }
    a.btn-log { color: #ffa726; text-decoration: none; font-weight: 600; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="badge-top">IoT Smart Agriculture</div>
      <h1>AGROSENSE PRO</h1>
      <p style="font-size: 12px; color: var(--text-dim); margin-top: 4px;">Hệ thống Quan trắc Đất & Cảnh báo Xâm nhập mặn</p>
    </header>

    <!-- Thẻ 1: Độ ẩm đất -->
    <div class="card">
      <div class="card-title">🌾 1. Độ Ẩm Đất Trồng (Soil Moisture)</div>
      <div class="metric-box">
        <div>
          <span class="metric-val" id="soil-val">--</span>
          <span class="metric-unit">%</span>
        </div>
        <span class="status-badge" id="soil-badge">Đang đọc...</span>
      </div>
      <div class="bar-bg">
        <div class="bar-fill" id="soil-bar"></div>
      </div>
      <div class="advice-text" id="soil-advice">Đang phân tích độ ẩm tầng đất...</div>
      <div class="raw-info">Chân GPIO 0 (ADC1_0) | Tín hiệu gốc: <span id="soil-adc">--</span> / 4095</div>
    </div>

    <!-- Thẻ 2: Chất lượng & Độ mặn nước -->
    <div class="card">
      <div class="card-title">🌊 2. Độ Mặn & Chất Lượng Nước Tưới (Salinity)</div>
      <div class="metric-box">
        <div>
          <span class="metric-val" id="water-val">--</span>
          <span class="metric-unit" id="water-unit">&permil;</span>
        </div>
        <span class="status-badge" id="water-badge">Đang đo...</span>
      </div>
      <div class="bar-bg">
        <div class="bar-fill" id="water-bar"></div>
      </div>
      <div class="advice-text" id="water-advice">Đang đo độ dẫn điện ion muối trong nước...</div>
      <div class="raw-info">Chân GPIO 1 (ADC1_1) | Tín hiệu dẫn điện: <span id="water-adc">--</span> / 4095</div>
    </div>

    <!-- Thẻ 3: Thông tin trạm & Mạng kết nối -->
    <div class="card">
      <div class="card-title">📡 3. Trạng Thái Hệ Thống</div>
      <div class="sys-grid">
        <div class="sys-item">
          <div class="sys-label">Mạng đã kết nối:</div>
          <div class="sys-value" id="net-ssid">Đang dò...</div>
        </div>
        <div class="sys-item">
          <div class="sys-label">Địa chỉ IP:</div>
          <div class="sys-value" id="net-ip">--</div>
        </div>
        <div class="sys-item">
          <div class="sys-label">Trạm phát AP:</div>
          <div class="sys-value" style="color: #64b5f6;">Esp32c3 - hệ thống modlun</div>
        </div>
        <div class="sys-item">
          <div class="sys-label">Nhiệt độ chip:</div>
          <div class="sys-value" id="chip-temp" style="color: #81c784;">-- &deg;C</div>
        </div>
      </div>
      <div style="margin-top: 12px; text-align: center;">
        <a href="/log" class="btn-log">📋 Xem Nhật Ký Đo Chi Tiết (Log)</a>
      </div>
    </div>

    <footer>
      Thiết bị mini cầm tay ESP32-C3 &bull; Tần số 80MHz siêu mát
    </footer>
  </div>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/api/data');
        if (!res.ok) return;
        const d = await res.json();

        // 1. Cap nhat Dat
        document.getElementById('soil-val').innerText = d.soil_pct;
        const sBar = document.getElementById('soil-bar');
        sBar.style.width = d.soil_pct + '%';
        let sColor = '#ff5252';
        if (d.soil_pct >= 25 && d.soil_pct <= 75) sColor = '#00e676';
        else if (d.soil_pct > 75) sColor = '#29b6f6';
        sBar.style.backgroundColor = sColor;
        const sBadge = document.getElementById('soil-badge');
        sBadge.innerText = d.soil_status;
        sBadge.style.color = sColor;
        document.getElementById('soil-adc').innerText = d.soil_adc;

        // 2. Cap nhat Nuoc
        const isDry = d.water_adc < 120;
        document.getElementById('water-val').innerText = isDry ? "KHÔ" : d.water_salinity.toFixed(1);
        document.getElementById('water-unit').innerHTML = isDry ? "" : "&permil;";
        const wBar = document.getElementById('water-bar');
        const pctBar = Math.min(100, Math.max(0, (d.water_adc / 3500.0) * 100));
        wBar.style.width = pctBar + '%';
        wBar.style.backgroundColor = d.water_color;
        const wBadge = document.getElementById('water-badge');
        wBadge.innerText = d.water_quality;
        wBadge.style.color = d.water_color;
        document.getElementById('water-advice').innerText = d.water_advice;
        document.getElementById('water-adc').innerText = d.water_adc;

        // 3. He thong
        document.getElementById('net-ssid').innerText = d.connected_net;
        document.getElementById('net-ip').innerText = d.ip_addr;
        document.getElementById('chip-temp').innerHTML = d.chip_temp.toFixed(1) + ' &deg;C';
      } catch (e) {
        console.error("Lỗi cập nhật dữ liệu:", e);
      }
    }
    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

// KẾT NỐI MẠNG KÉP TỰ HÀNH: Kết nối mạng nào thì lập tức dừng dò tìm!
void connectDualWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);

  // Khởi động trạm SoftAP độc lập trước (Luôn luôn sẵn sàng tại 192.168.4.1)
  WiFi.softAP(AP_SSID);
  addLog("📡 Trạm phát AP đã mở: '" + String(AP_SSID) + "' (IP: 192.168.4.1)");

  // Quét và kết nối ưu tiên mạng nào có sẵn
  for (int i = 0; i < NUM_NETWORKS; i++) {
    const char* targetSSID = NETWORKS[i].ssid;
    const char* targetPass = NETWORKS[i].pass;

    addLog("🔍 Đang thử kết nối mạng [" + String(i + 1) + "/" + String(NUM_NETWORKS) + "]: " + String(targetSSID));
    WiFi.begin(targetSSID, targetPass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < 4500)) {
      delay(150);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Chớp LED nhẹ
    }

    if (WiFi.status() == WL_CONNECTED) {
      isStationConnected = true;
      connectedSSID = String(targetSSID);
      localIPStr = WiFi.localIP().toString();
      digitalWrite(LED_PIN, LOW); // Đèn xanh sáng đứng yên
      addLog("🎉 KẾT NỐI THÀNH CÔNG VÀO MẠNG: " + connectedSSID + " (IP: " + localIPStr + ")");
      addLog("🛑 ĐÃ DỪNG VIỆC DÒ TÌM MẠNG THEO ĐÚNG CHỈ THỊ!");
      return; // DỪNG NGAY LẬP TỨC!
    } else {
      WiFi.disconnect();
      delay(100);
    }
  }

  // Nếu cả 2 mạng đều không có (ở ngoài vườn / trường học):
  isStationConnected = false;
  connectedSSID = "Chưa kết nối (Chạy độc lập AP)";
  localIPStr = "192.168.4.1";
  digitalWrite(LED_PIN, LOW);
  addLog("⚠️ Cả 2 mạng ngoài đều vắng mặt. Hệ thống hoạt động độc lập qua AP: " + String(AP_SSID));
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // Không bị treo khi dùng pin
  setCpuFrequencyMhz(80);   // Mát mẻ 38-40C

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  analogReadResolution(12);

  // Kích hoạt kết nối kép
  connectDualWiFi();

  // Đọc cảm biến mẫu đầu tiên
  computeSoilMoisture(readPrecisionADC(SOIL_PIN));
  computeWaterSalinity(readPrecisionADC(WATER_PIN));

  // --- CÁC ENDPOINT WEB ---
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/api/data", HTTP_GET, []() {
    float temp = temperatureRead();
    String json = "{";
    json += ""soil_pct":" + String(soilPercent) + ",";
    json += ""soil_adc":" + String(soilADC) + ",";
    json += ""soil_status":"" + soilStatus + "",";
    json += ""water_salinity":" + String(waterSalinity, 2) + ",";
    json += ""water_adc":" + String(waterADC) + ",";
    json += ""water_quality":"" + waterQuality + "",";
    json += ""water_advice":"" + waterAdvice + "",";
    json += ""water_color":"" + waterColor + "",";
    json += ""chip_temp":" + String(temp, 1) + ",";
    json += ""connected_net":"" + connectedSSID + "",";
    json += ""ip_addr":"" + localIPStr + """;
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/soil", HTTP_GET, []() {
    server.send(200, "text/plain", String(soilPercent));
  });

  server.on("/water", HTTP_GET, []() {
    server.send(200, "text/plain", String(waterSalinity, 2));
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "SoilADC=" + String(soilADC) + " (" + String(soilPercent) + "%) | WaterADC=" + String(waterADC) + " (" + String(waterSalinity, 2) + " permil)";
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Đất: " + String(soilPercent) + "% | Nước: " + String(waterSalinity, 2) + " ‰ | Nhiệt chip: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  // Cổng Web OTA nạp code không dây
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
  addLog("🚀 Web Server đã bật trên cả mạng LAN và mạng AP!");
}

void loop() {
  server.handleClient();

  // Kiểm tra rớt mạng định kỳ (nếu trước đó đã kết nối)
  static unsigned long lastNetCheck = 0;
  if (isStationConnected && millis() - lastNetCheck > 8000) {
    lastNetCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      addLog("⚠️ Mất kết nối mạng " + connectedSSID + "! Đang tự động kết nối lại...");
      WiFi.reconnect();
    }
  }

  // Đo đạc cảm biến chính xác mỗi 350ms
  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  if (now - lastMeasure >= 350) {
    lastMeasure = now;

    int sADC = readPrecisionADC(SOIL_PIN);
    computeSoilMoisture(sADC);

    int wADC = readPrecisionADC(WATER_PIN);
    computeWaterSalinity(wADC);

    // Ghi nhận log khi nước thay đổi trạng thái
    static String lastQuality = "";
    if (waterQuality != lastQuality) {
      lastQuality = waterQuality;
      addLog("🌊 Nguồn nước thay đổi: " + waterQuality + " (" + String(waterSalinity, 1) + " ‰)");
    }
  }

  delay(5);
}
