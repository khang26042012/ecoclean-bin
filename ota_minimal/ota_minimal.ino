/*
 * ESP32 DEVKIT V1 - BLUE LED ON & WEB OTA FIRMWARE
 * - Bo mach: ESP32 DevKit V1 (ESP-WROOM-32, Dual-Core 240MHz)
 * - Den LED xanh: GPIO 2 sang lien tuc 100%
 * - Wi-Fi: Tu dong ket noi "Trong Khang" / "phantrongkhangg"
 * - Web OTA: Cap nhat firmware khong day qua http://<IP>/update
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_wifi.h>

#define LED_PIN 2 // Den LED xanh tren ESP32 DevKit V1 (GPIO 2)

WebServer server(80);

struct WiFiCred {
  const char* ssid;
  const char* pass;
};

const WiFiCred NETWORKS[] = {
  {"Trong Khang", "khongbiet123"},
  {"phantrongkhangg", "26042012khang"}
};
const int NUM_NETWORKS = sizeof(NETWORKS) / sizeof(NETWORKS[0]);

String connectedSSID = "Dang ket noi...";
String localIP = "0.0.0.0";
unsigned long lastHeartbeat = 0;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 DevKit V1 - Sẵn Sàng</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #1e293b; border-radius: 16px; padding: 28px; width: 100%; max-width: 440px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }
    .header { text-align: center; margin-bottom: 24px; }
    .badge { display: inline-flex; align-items: center; gap: 8px; background: rgba(59, 130, 246, 0.15); color: #60a5fa; padding: 6px 14px; border-radius: 20px; font-size: 13px; font-weight: 600; border: 1px solid rgba(59, 130, 246, 0.3); }
    .dot { width: 10px; height: 10px; background: #3b82f6; border-radius: 50%; box-shadow: 0 0 12px #3b82f6; animation: pulse 2s infinite; }
    @keyframes pulse { 0% { opacity: 0.5; } 50% { opacity: 1; transform: scale(1.1); } 100% { opacity: 0.5; } }
    h1 { font-size: 22px; margin-top: 14px; color: #ffffff; }
    .info-box { background: #0f172a; border-radius: 12px; padding: 16px; margin-bottom: 20px; border: 1px solid #1e293b; }
    .info-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #1e293b; font-size: 14px; }
    .info-row:last-child { border-bottom: none; }
    .info-label { color: #94a3b8; }
    .info-val { font-weight: 600; color: #e2e8f0; }
    .btn { display: block; width: 100%; text-align: center; background: #2563eb; color: #ffffff; padding: 14px; border-radius: 12px; text-decoration: none; font-weight: 600; font-size: 15px; transition: 0.2s; box-shadow: 0 4px 14px rgba(37, 99, 235, 0.4); }
    .btn:hover { background: #1d4ed8; }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="badge"><div class="dot"></div> ĐÈN XANH GPIO 2: ĐANG SÁNG</div>
      <h1>ESP32 DevKit V1</h1>
      <p style="color: #94a3b8; font-size: 13px; margin-top: 6px;">Dual-Core 240MHz &bull; OTA Ready</p>
    </div>

    <div class="info-box">
      <div class="info-row">
        <span class="info-label">Trạng thái Wi-Fi:</span>
        <span class="info-val" id="wifi-ssid">%SSID%</span>
      </div>
      <div class="info-row">
        <span class="info-label">Địa chỉ IP Local:</span>
        <span class="info-val" style="color: #38bdf8;">%IP%</span>
      </div>
      <div class="info-row">
        <span class="info-label">Tín hiệu RSSI:</span>
        <span class="info-val">%RSSI% dBm</span>
      </div>
      <div class="info-row">
        <span class="info-label">RAM trống (Heap):</span>
        <span class="info-val" style="color: #4ade80;">%HEAP% KB</span>
      </div>
      <div class="info-row">
        <span class="info-label">Cổng nạp OTA:</span>
        <span class="info-val">/update (Sẵn sàng)</span>
      </div>
    </div>

    <a href="/update" class="btn">🚀 Nạp Code Mới Qua Wi-Fi (OTA)</a>
  </div>
</body>
</html>
)rawliteral";

const char UPDATE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Cập Nhật Firmware OTA</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #1e293b; border-radius: 16px; padding: 28px; width: 100%; max-width: 440px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }
    h1 { font-size: 20px; text-align: center; margin-bottom: 20px; color: #38bdf8; }
    .upload-area { border: 2px dashed #475569; border-radius: 12px; padding: 24px; text-align: center; margin-bottom: 20px; background: #0f172a; }
    input[type=file] { width: 100%; color: #94a3b8; font-size: 14px; }
    .btn { display: block; width: 100%; background: #059669; color: #ffffff; border: none; padding: 14px; border-radius: 12px; font-weight: 600; font-size: 15px; cursor: pointer; transition: 0.2s; box-shadow: 0 4px 14px rgba(5, 150, 105, 0.4); }
    .btn:hover { background: #047857; }
    .progress-bar { width: 100%; background: #334155; height: 12px; border-radius: 6px; overflow: hidden; margin-top: 20px; display: none; }
    .progress-fill { height: 100%; background: #38bdf8; width: 0%; transition: width 0.1s; }
    #status { text-align: center; margin-top: 14px; font-size: 14px; color: #94a3b8; }
    .back-link { display: block; text-align: center; margin-top: 16px; color: #64748b; text-decoration: none; font-size: 13px; }
    .back-link:hover { color: #94a3b8; }
  </style>
</head>
<body>
  <div class="card">
    <h1>🚀 Nạp Firmware Qua Wi-Fi</h1>
    <form id="upload-form" method="POST" action="/update" enctype="multipart/form-data">
      <div class="upload-area">
        <p style="font-size: 14px; color: #cbd5e1; margin-bottom: 12px;">Chọn file <b>firmware.bin</b></p>
        <input type="file" name="update" id="file-input" accept=".bin" required>
      </div>
      <button type="submit" class="btn" id="btn-submit">Bắt Đầu Nạp Firmware</button>
      <div class="progress-bar" id="progress-bar">
        <div class="progress-fill" id="progress-fill"></div>
      </div>
      <div id="status"></div>
    </form>
    <a href="/" class="back-link">&larr; Quay lại trang chủ</a>
  </div>

  <script>
    const form = document.getElementById('upload-form');
    const fileInput = document.getElementById('file-input');
    const btn = document.getElementById('btn-submit');
    const pBar = document.getElementById('progress-bar');
    const pFill = document.getElementById('progress-fill');
    const statusDiv = document.getElementById('status');

    form.onsubmit = function(e) {
      e.preventDefault();
      const file = fileInput.files[0];
      if (!file) return;

      btn.disabled = true;
      btn.style.opacity = '0.5';
      pBar.style.display = 'block';
      statusDiv.innerHTML = 'Đang tải file lên ESP32...';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update');

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          pFill.style.width = percent + '%';
          statusDiv.innerHTML = 'Đang nạp: ' + percent + '%';
        }
      };

      xhr.onload = function() {
        if (xhr.status === 200) {
          pFill.style.width = '100%';
          pFill.style.background = '#22c55e';
          statusDiv.innerHTML = '<b style="color: #22c55e;">✅ Nạp thành công!</b><br>ESP32 đang khởi động lại (10s)...';
          setTimeout(() => { window.location.href = '/'; }, 8000);
        } else {
          statusDiv.innerHTML = '<b style="color: #ef4444;">❌ Lỗi nạp: ' + xhr.responseText + '</b>';
          btn.disabled = false;
          btn.style.opacity = '1';
        }
      };

      xhr.onerror = function() {
        statusDiv.innerHTML = '<b style="color: #ef4444;">❌ Lỗi kết nối mạng!</b>';
        btn.disabled = false;
        btn.style.opacity = '1';
      };

      const data = new FormData();
      data.append('update', file);
      xhr.send(data);
    };
  </script>
</body>
</html>
)rawliteral";

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("");
  Serial.println("[WiFi] Bat dau ket noi Wi-Fi nha...");

  for (int i = 0; i < NUM_NETWORKS; i++) {
    Serial.print("[WiFi] Thu mang: ");
    Serial.print(NETWORKS[i].ssid);
    Serial.println(" ...");
    WiFi.begin(NETWORKS[i].ssid, NETWORKS[i].pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED) {
      connectedSSID = NETWORKS[i].ssid;
      localIP = WiFi.localIP().toString();
      Serial.println("==================================================");
      Serial.print("[WiFi] >>> KET NOI THANH CONG: ");
      Serial.println(connectedSSID);
      Serial.print("[WiFi] >>> DIA CHI IP: http://");
      Serial.print(localIP);
      Serial.println("/ <<<");
      Serial.print("[OTA]  >>> TRANG NAP CODE: http://");
      Serial.print(localIP);
      Serial.println("/update <<<");
      Serial.println("==================================================");
      return;
    }
  }

  Serial.println("[WiFi] Khong ket noi duoc mang nao. Se thu lai...");
}

void handleRoot() {
  String html = FPSTR(INDEX_HTML);
  html.replace("%SSID%", connectedSSID);
  html.replace("%IP%", localIP);
  html.replace("%RSSI%", String(WiFi.RSSI()));
  html.replace("%HEAP%", String(ESP.getFreeHeap() / 1024));
  server.send(200, "text/html; charset=utf-8", html);
}

void handleUpdatePage() {
  server.send(200, "text/html; charset=utf-8", FPSTR(UPDATE_HTML));
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. BAT DEN XANH GPIO 2 SANG LIEN TUC
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Sang den xanh 100%

  Serial.println("");
  Serial.println("==================================================");
  Serial.println("=== ESP32 DEVKIT V1 - BLUE LED ON & WEB OTA ===");
  Serial.println("=== Chip: ESP32 Dual-Core | LED GPIO: 2 (ON)  ===");
  Serial.println("==================================================");

  // 2. KET NOI WI-FI NHA
  connectToWiFi();

  // 3. CAU HINH WEB SERVER & OTA
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.print("[OTA] Bat dau nhan file: ");
      Serial.println(upload.filename);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.print("[OTA] Nap hoan tat! Dung luong: ");
        Serial.print(upload.totalSize);
        Serial.println(" bytes. Dang khoi dong lai...");
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("[System] Web Server da khoi dong tren port 80!");
}

void loop() {
  server.handleClient();

  // Tu dong ket noi lai neu rot mang
  if (WiFi.status() != WL_CONNECTED && millis() - lastHeartbeat > 10000) {
    connectToWiFi();
  }

  // Heartbeat moi 5 giay gui qua Serial cho app Terminal
  if (millis() - lastHeartbeat >= 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_PIN, HIGH); // Dam bao den xanh luon sang
    Serial.print("[Heartbeat] IP: ");
    Serial.print(localIP);
    Serial.print(" | Free Heap: ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB | Blue LED: ON");
  }
}
