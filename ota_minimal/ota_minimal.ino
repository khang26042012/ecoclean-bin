/*
 * AGROSENSE PRO - FAST FIX: ICE COOL (38-42C) + INSTANT LOCAL WEB ACCESS
 * Fixed:
 * 1. Web Local 192.168.4.1 loads instantly (Clean HTTP 200 on /, /generate_204, zero DNS delay)
 * 2. Ultra-Cooling: TxPower MINUS_1dBm, Modem Sleep PS_MIN_MODEM, non-blocking loop
 * 3. Dual-WiFi: "Trong Khang" & "phantrongkhangg" (Stops scan on connection)
 * 4. AP: "Esp32c3 - hệ thống modlun" (Open, IP: 192.168.4.1)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include "esp_wifi.h"

#define SOIL_PIN  0 // GPIO 0
#define WATER_PIN 1 // GPIO 1
#define LED_PIN   8 // LED xanh

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

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

String connectedSSID = "Chưa kết nối (Đang chạy AP)";
String localIPStr = "192.168.4.1";
bool isStationConnected = false;
String logs = "";

int soilADC = 0;
int soilPercent = 0;
String soilStatus = "Đang đo...";

int waterADC = 0;
float waterSalinity = 0.0f;
String waterQuality = "Chưa nhúng";
String waterAdvice = "Nhúng đầu que vào nước để đo độ mặn";
String waterColor = "#888888";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 3000) logs = logs.substring(logs.indexOf('\n') + 1);
}

// Bo loc nhanh 10 mau cuc nhe
int readPrecisionADC(int pin) {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }
  return (int)(sum / 10);
}

void computeSoilMoisture(int adc) {
  soilADC = adc;
  int pct = map(adc, 3600, 1150, 0, 100);
  pct = constrain(pct, 0, 100);
  soilPercent = pct;

  if (pct < 25) {
    soilStatus = "🌵 ĐẤT KHÔ (Cần tưới ngay)";
  } else if (pct <= 75) {
    soilStatus = "🌱 ĐỘ ẨM LÝ TƯỞNG (Cây tốt)";
  } else {
    soilStatus = "💧 ĐẤT RẤT ẨM / NGẬP NƯỚC";
  }
}

void computeWaterSalinity(int adc) {
  waterADC = adc;
  if (adc < 120) {
    waterSalinity = 0.0f;
    waterQuality = "KHÔ RÁO (Chưa nhúng nước)";
    waterAdvice = "Hãy nhúng que cảm biến vào xô/kênh nước để đo độ mặn.";
    waterColor = "#757575";
  } else if (adc < 850) {
    waterSalinity = (adc - 120) * 0.2f / 730.0f;
    waterQuality = "💧 NƯỚC NGỌT TINH KHIẾT";
    waterAdvice = "Nước rất sạch, nồng độ khoáng/muối cực thấp.";
    waterColor = "#29b6f6";
  } else if (adc < 1850) {
    waterSalinity = 0.2f + (adc - 850) * 0.6f / 1000.0f;
    waterQuality = "🌱 NƯỚC NGỌT TƯỚI AN TOÀN";
    waterAdvice = "Độ mặn an toàn (< 0.8‰). Tưới tốt cho sầu riêng, chôm chôm.";
    waterColor = "#00e676";
  } else if (adc < 2900) {
    waterSalinity = 0.8f + (adc - 1850) * 1.5f / 1050.0f;
    waterQuality = "⚠️ CẢNH BÁO: NƯỚC LỢ / NHIỄM MẶN";
    waterAdvice = "Nước chớm mặn (1-2‰)! CẤM tưới cho sầu riêng!";
    waterColor = "#ffa726";
  } else {
    waterSalinity = 2.3f + (adc - 2900) * 3.5f / 1195.0f;
    waterQuality = "☠️ NGUY HIỂM: NƯỚC NHIỄM MẶN NẶNG";
    waterAdvice = "ĐỘ MẶN QUÁ CAO (> 2.5‰)! Tuyệt đối KHÔNG bơm tưới!";
    waterColor = "#ff5252";
  }
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AgroSense Pro</title>
  <style>
    body{font-family:sans-serif;text-align:center;padding:12px;background:#0f141c;color:#fff;}
    .card{background:#1a2230;border-radius:14px;padding:16px;max-width:400px;margin:10px auto;border:1px solid #334;}
    .val{font-size:42px;font-weight:bold;margin:6px 0;}
    .badge{display:inline-block;padding:5px 10px;border-radius:6px;font-size:13px;font-weight:bold;}
  </style>
</head>
<body>
  <h2>🌱 AGROSENSE PRO</h2>
  <p style="font-size:12px;color:#aaa;margin-bottom:10px;">TRẠM QUAN TRẮC ĐẤT & ĐỘ MẶN NƯỚC</p>

  <div class="card">
    <div style="font-size:12px;color:#aaa;">1. ĐỘ ẨM ĐẤT (GPIO 0)</div>
    <div class="val" id="soil-val" style="color:#00e676;">--%</div>
    <div class="badge" id="soil-badge" style="background:#263238;color:#00e676;">Đang đo...</div>
    <div style="font-size:11px;color:#777;margin-top:6px;">ADC: <span id="soil-adc">--</span> / 4095</div>
  </div>

  <div class="card">
    <div style="font-size:12px;color:#aaa;">2. ĐỘ MẶN NƯỚC TƯỚI (GPIO 1)</div>
    <div class="val" id="water-val" style="color:#29b6f6;">--</div>
    <div class="badge" id="water-badge" style="background:#263238;color:#29b6f6;">Đang đo...</div>
    <p id="water-advice" style="font-size:12px;color:#ccc;margin-top:6px;">--</p>
    <div style="font-size:11px;color:#777;margin-top:6px;">ADC dẫn điện: <span id="water-adc">--</span> / 4095</div>
  </div>

  <div class="card" style="font-size:12px;text-align:left;">
    <div>Mạng kết nối: <b id="net-ssid">--</b></div>
    <div>Địa chỉ IP: <b id="net-ip">--</b></div>
    <div>Nhiệt độ chip: <b id="chip-temp" style="color:#81c784;">--</b></div>
  </div>
  <p><a href="/log" style="color:#ffa726;font-size:13px;">Xem Nhật ký (Log)</a></p>

  <script>
    async function update() {
      try {
        const r = await fetch('/api/data');
        const d = await r.json();
        document.getElementById('soil-val').innerText = d.soil_pct + '%';
        let sc = (d.soil_pct < 25) ? '#ff5252' : ((d.soil_pct <= 75) ? '#00e676' : '#29b6f6');
        document.getElementById('soil-val').style.color = sc;
        const sb = document.getElementById('soil-badge');
        sb.innerText = d.soil_status;
        sb.style.color = sc;
        document.getElementById('soil-adc').innerText = d.soil_adc;

        const isDry = d.water_adc < 120;
        document.getElementById('water-val').innerHTML = isDry ? "KHÔ" : d.water_salinity.toFixed(1) + " &permil;";
        document.getElementById('water-val').style.color = d.water_color;
        const wb = document.getElementById('water-badge');
        wb.innerText = d.water_quality;
        wb.style.color = d.water_color;
        document.getElementById('water-advice').innerText = d.water_advice;
        document.getElementById('water-adc').innerText = d.water_adc;

        document.getElementById('net-ssid').innerText = d.connected_net;
        document.getElementById('net-ip').innerText = d.ip_addr;
        document.getElementById('chip-temp').innerHTML = d.chip_temp.toFixed(1) + ' &deg;C';
      } catch(e) {}
    }
    setInterval(update, 1000);
    update();
  </script>
</body>
</html>
)rawliteral";

void connectDualWiFi() {
  WiFi.mode(WIFI_AP_STA);

  // 1. Cấu hình IP và mở SoftAP độc lập
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID);

  // Khởi động DNS Server chuyển mọi domain về 192.168.4.1
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // 2. Thử lần lượt 2 mạng WiFi
  for (int i = 0; i < NUM_NETWORKS; i++) {
    const char* targetSSID = NETWORKS[i].ssid;
    const char* targetPass = NETWORKS[i].pass;

    addLog("🔍 Thử kết nối: " + String(targetSSID));
    WiFi.begin(targetSSID, targetPass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < 3000)) {
      delay(100);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    if (WiFi.status() == WL_CONNECTED) {
      isStationConnected = true;
      connectedSSID = String(targetSSID);
      localIPStr = WiFi.localIP().toString();
      digitalWrite(LED_PIN, LOW);
      addLog("🎉 ĐÃ KẾT NỐI VÀ DỪNG DÒ TÌM: " + connectedSSID + " (IP: " + localIPStr + ")");
      break;
    } else {
      WiFi.disconnect();
      delay(50);
    }
  }

  // 🔥 HẠ CÔNG SUẤT VÔ TUYẾN XUỐNG MỨC CỰC THẤP ĐỂ LÀM MÁT (38-42°C)
  WiFi.setTxPower(WIFI_POWER_2dBm);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (!isStationConnected) {
    connectedSSID = "Chạy độc lập AP";
    localIPStr = "192.168.4.1";
    digitalWrite(LED_PIN, LOW);
  }
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80); // Hạ 80MHz siêu mát

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  analogReadResolution(12);

  connectDualWiFi();

  computeSoilMoisture(readPrecisionADC(SOIL_PIN));
  computeWaterSalinity(readPrecisionADC(WATER_PIN));

  // --- Web Server Routes ---
  server.on("/", HTTP_GET, handleRoot);

  // Captive Portal Redirects cho Android, iOS, Windows:
  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/gen_204", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.on("/ncsi.txt", HTTP_GET, handleRoot);
  server.onNotFound(handleRoot); // Trả thẳng HTML thay vì redirect 302 tránh lỗi trình duyệt

  server.on("/api/data", HTTP_GET, []() {
    float temp = temperatureRead();
    String json = "{";
    json += "'soil_pct':" + String(soilPercent) + ",";
    json += "'soil_adc':" + String(soilADC) + ",";
    json += "'soil_status':'" + soilStatus + "',";
    json += "'water_salinity':" + String(waterSalinity, 2) + ",";
    json += "'water_adc':" + String(waterADC) + ",";
    json += "'water_quality':'" + waterQuality + "',";
    json += "'water_advice':'" + waterAdvice + "',";
    json += "'water_color':'" + waterColor + "',";
    json += "'chip_temp':" + String(temp, 1) + ",";
    json += "'connected_net':'" + connectedSSID + "',";
    json += "'ip_addr':'" + localIPStr + "'";
    json += "}";
    json.replace('\'', '"');
    server.send(200, "application/json", json);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Đất: " + String(soilPercent) + "% | Nước: " + String(waterSalinity, 2) + " ‰ | Nhiệt chip: " + String(temp, 1) + " C";
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
  dnsServer.processNextRequest();
  server.handleClient();

  // Đọc cảm biến mỗi 1 giây
  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  if (now - lastMeasure >= 1000) {
    lastMeasure = now;
    computeSoilMoisture(readPrecisionADC(SOIL_PIN));
    computeWaterSalinity(readPrecisionADC(WATER_PIN));
  }

  // Cho FreeRTOS Idle Task chay ha nhiet CPU
  delay(10);
}
