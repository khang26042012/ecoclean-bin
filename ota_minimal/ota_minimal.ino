/*
 * ESP32 DEVKIT V1 - HIGH PERFORMANCE RESIDENTIAL SOCKS5 PROXY
 * - Bo mach: ESP32 DevKit V1 (ESP-WROOM-32, Dual-Core 240MHz)
 * - Giao thuc: SOCKS5 (RFC 1928) + User/Password Auth (RFC 1929)
 * - Bao mat: Bat buoc xac thuc Username / Password (chong quet do, chong nat mang)
 * - Tich hop: Web Dashboard (Port 80) + Web OTA (/update)
 * - Den LED xanh: GPIO 2 sang lien tuc
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_wifi.h>

#define LED_PIN 2

// ================= THIET LAP PROXY & BAO MAT =================
const uint16_t SOCKS5_PORT = 1080;
const char* PROXY_USER = "khang";
const char* PROXY_PASS = "khang2604";

// ================= THIET LAP WI-FI NHA =================
struct WiFiCred {
  const char* ssid;
  const char* pass;
};

const WiFiCred NETWORKS[] = {
  {"Trong Khang", "khongbiet123"},
  {"phantrongkhangg", "26042012khang"}
};
const int NUM_NETWORKS = sizeof(NETWORKS) / sizeof(NETWORKS[0]);

// ================= BIEN HE THONG =================
WebServer server(80);
WiFiServer socksServer(SOCKS5_PORT);

String connectedSSID = "Dang ket noi...";
String localIP = "0.0.0.0";
volatile int activeSessions = 0;
volatile unsigned long totalBytesRelayed = 0;
unsigned long lastHeartbeat = 0;

float readChipTemp() {
  float t = temperatureRead();
  if (t < 10.0 || t > 120.0) t = 31.5;
  return t;
}

// Cấu trúc phiên kết nối Proxy
struct ProxySession {
  WiFiClient client;
};

// Luồng xử lý một phiên SOCKS5 (chạy đa nhiệm FreeRTOS)
void socksWorkerTask(void* param) {
  ProxySession* session = (ProxySession*)param;
  WiFiClient client = session->client;
  delete session; // Giải phóng struct giữ con trỏ

  WiFiClient target;
  client.setTimeout(10); // 10 giay timeout cho handshake
  target.setTimeout(10);

  // 1. SOCKS5 GREETING (RFC 1928)
  uint8_t ver = client.read();
  if (ver != 0x05) {
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  uint8_t nmethods = client.read();
  bool supportsUserPass = false;
  for (int i = 0; i < nmethods; i++) {
    uint8_t m = client.read();
    if (m == 0x02) supportsUserPass = true; // 0x02: USERNAME/PASSWORD
  }

  if (!supportsUserPass) {
    uint8_t noAcceptable[] = {0x05, 0xFF};
    client.write(noAcceptable, 2);
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  // Phản hồi yêu cầu xác thực User/Password
  uint8_t methodResp[] = {0x05, 0x02};
  client.write(methodResp, 2);

  // 2. USERNAME/PASSWORD SUB-NEGOTIATION (RFC 1929)
  unsigned long authStart = millis();
  while (!client.available() && millis() - authStart < 5000) delay(10);
  if (!client.available()) {
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  uint8_t authVer = client.read();
  if (authVer != 0x01) {
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  uint8_t ulen = client.read();
  char userBuf[64] = {0};
  for (int i = 0; i < ulen && i < 63; i++) userBuf[i] = (char)client.read();

  uint8_t plen = client.read();
  char passBuf[64] = {0};
  for (int i = 0; i < plen && i < 63; i++) passBuf[i] = (char)client.read();

  // Kiem tra User & Password
  if (strcmp(userBuf, PROXY_USER) != 0 || strcmp(passBuf, PROXY_PASS) != 0) {
    // Sai pass -> tu choi va dong ket noi ngay lap tuc
    uint8_t authFail[] = {0x01, 0xFF};
    client.write(authFail, 2);
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  // Dung pass -> Bao thanh cong (Status 0x00)
  uint8_t authSuccess[] = {0x01, 0x00};
  client.write(authSuccess, 2);

  // 3. SOCKS5 REQUEST (RFC 1928)
  unsigned long reqStart = millis();
  while (!client.available() && millis() - reqStart < 5000) delay(10);
  if (!client.available()) {
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  uint8_t reqVer = client.read();
  uint8_t cmd = client.read();
  uint8_t rsv = client.read();
  uint8_t atyp = client.read();

  if (reqVer != 0x05 || cmd != 0x01) { // 0x01 = CONNECT
    uint8_t cmdUnsupported[] = {0x05, 0x07, 0x00, 0x01, 0,0,0,0, 0,0};
    client.write(cmdUnsupported, 10);
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  char targetHost[128] = {0};
  uint16_t targetPort = 0;

  if (atyp == 0x01) { // IPv4
    uint8_t ip[4];
    for (int i = 0; i < 4; i++) ip[i] = client.read();
    snprintf(targetHost, sizeof(targetHost), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  } else if (atyp == 0x03) { // Domain Name
    uint8_t domainLen = client.read();
    for (int i = 0; i < domainLen && i < 127; i++) targetHost[i] = (char)client.read();
  } else {
    // IPv6 or unsupported
    uint8_t atypUnsupported[] = {0x05, 0x08, 0x00, 0x01, 0,0,0,0, 0,0};
    client.write(atypUnsupported, 10);
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  targetPort = (client.read() << 8) | client.read();

  // 4. KET NOI TOI SERVER DICH
  if (!target.connect(targetHost, targetPort, 5000)) {
    uint8_t connFail[] = {0x05, 0x04, 0x00, 0x01, 0,0,0,0, 0,0}; // 0x04 = Host unreachable
    client.write(connFail, 10);
    client.stop();
    vTaskDelete(NULL);
    return;
  }

  // Phan hoi ket noi thanh cong
  uint8_t connSuccess[] = {0x05, 0x00, 0x00, 0x01, 0,0,0,0, 0,0};
  client.write(connSuccess, 10);

  // 5. CHUYEN TIEP DU LIEU 2 CHIEU (BIDIRECTIONAL RELAY)
  activeSessions++;
  uint8_t buffer[2048];
  unsigned long lastActivity = millis();

  while (client.connected() && target.connected()) {
    bool hasData = false;

    // Tu Client -> Target
    int bytesAvailable = client.available();
    if (bytesAvailable > 0) {
      int bytesToRead = (bytesAvailable > sizeof(buffer)) ? sizeof(buffer) : bytesAvailable;
      int bytesRead = client.read(buffer, bytesToRead);
      if (bytesRead > 0) {
        target.write(buffer, bytesRead);
        totalBytesRelayed += bytesRead;
        hasData = true;
      }
    }

    // Tu Target -> Client
    bytesAvailable = target.available();
    if (bytesAvailable > 0) {
      int bytesToRead = (bytesAvailable > sizeof(buffer)) ? sizeof(buffer) : bytesAvailable;
      int bytesRead = target.read(buffer, bytesToRead);
      if (bytesRead > 0) {
        client.write(buffer, bytesRead);
        totalBytesRelayed += bytesRead;
        hasData = true;
      }
    }

    if (hasData) {
      lastActivity = millis();
    } else {
      // Timeout 60 giay khong co du lieu -> ngat ket noi
      if (millis() - lastActivity > 60000) break;
      vTaskDelay(5 / portTICK_PERIOD_MS); // Nhuong CPU cho cac task khac
    }
  }

  client.stop();
  target.stop();
  activeSessions--;
  vTaskDelete(NULL);
}

// ================= GIAO DIEN WEB DASHBOARD =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Khang Residential SOCKS5 Proxy</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #090d16; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #131b2e; border-radius: 20px; padding: 28px; width: 100%; max-width: 480px; box-shadow: 0 15px 35px rgba(0,0,0,0.6); border: 1px solid #1e293b; }
    .header { text-align: center; margin-bottom: 24px; }
    .badge { display: inline-flex; align-items: center; gap: 8px; background: rgba(34, 197, 94, 0.15); color: #4ade80; padding: 6px 16px; border-radius: 20px; font-size: 13px; font-weight: 600; border: 1px solid rgba(34, 197, 94, 0.3); }
    .dot { width: 10px; height: 10px; background: #22c55e; border-radius: 50%; box-shadow: 0 0 12px #22c55e; animation: pulse 2s infinite; }
    @keyframes pulse { 0% { opacity: 0.5; } 50% { opacity: 1; transform: scale(1.1); } 100% { opacity: 0.5; } }
    h1 { font-size: 22px; margin-top: 14px; color: #ffffff; }
    .subtitle { color: #94a3b8; font-size: 13px; margin-top: 4px; }
    
    .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 20px; }
    .stat-box { background: #0b1120; border-radius: 14px; padding: 16px; text-align: center; border: 1px solid #1e293b; }
    .stat-val { font-size: 24px; font-weight: 700; color: #38bdf8; }
    .stat-lbl { font-size: 12px; color: #94a3b8; margin-top: 4px; }
    
    .info-box { background: #0b1120; border-radius: 14px; padding: 18px; margin-bottom: 20px; border: 1px solid #1e293b; }
    .info-title { font-size: 14px; font-weight: 600; color: #f59e0b; margin-bottom: 12px; display: flex; align-items: center; gap: 6px; }
    .info-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #1e293b; font-size: 13px; }
    .info-row:last-child { border-bottom: none; }
    .info-label { color: #94a3b8; }
    .info-val { font-weight: 600; color: #e2e8f0; }

    .code-box { background: #000000; border-radius: 10px; padding: 12px; font-family: monospace; font-size: 12px; color: #4ade80; word-break: break-all; margin-top: 8px; border: 1px solid #334155; }
    
    .btn { display: block; width: 100%; text-align: center; background: #2563eb; color: #ffffff; padding: 14px; border-radius: 12px; text-decoration: none; font-weight: 600; font-size: 14px; transition: 0.2s; box-shadow: 0 4px 14px rgba(37, 99, 235, 0.4); margin-top: 14px; }
    .btn:hover { background: #1d4ed8; }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="badge"><div class="dot"></div> PROXY SOCKS5: ĐANG HOẠT ĐỘNG</div>
      <h1>Khang Residential Proxy</h1>
      <p class="subtitle">IP Dân Cư Sạch &bull; ESP32 DevKit V1 240MHz</p>
    </div>

    <div class="stats-grid">
      <div class="stat-box">
        <div class="stat-val" id="temp-val">%TEMP% &deg;C</div>
        <div class="stat-lbl">❄️ Nhiệt Độ Lõi CPU</div>
      </div>
      <div class="stat-box">
        <div class="stat-val" id="sessions-val">%SESSIONS%</div>
        <div class="stat-lbl">🔗 Luồng Đang Kết Nối</div>
      </div>
    </div>

    <div class="info-box">
      <div class="info-title">🔐 THÔNG TIN ĐĂNG NHẬP PROXY</div>
      <div class="info-row">
        <span class="info-label">Giao thức:</span>
        <span class="info-val" style="color: #4ade80;">SOCKS5 (RFC 1928)</span>
      </div>
      <div class="info-row">
        <span class="info-label">Cổng (Port):</span>
        <span class="info-val" style="color: #38bdf8;">1080</span>
      </div>
      <div class="info-row">
        <span class="info-label">Tài khoản (User):</span>
        <span class="info-val">%USER%</span>
      </div>
      <div class="info-row">
        <span class="info-label">Mật khẩu (Pass):</span>
        <span class="info-val" style="color: #f43f5e;">%PASS%</span>
      </div>
      <div style="margin-top: 12px; font-size: 12px; color: #94a3b8;">Chuỗi kết nối cấu hình nhanh:</div>
      <div class="code-box">socks5://%USER%:%PASS%@%IP%:1080</div>
    </div>

    <div class="info-box">
      <div class="info-title">📊 HỆ THỐNG & WI-FI</div>
      <div class="info-row">
        <span class="info-label">Wi-Fi mạng nhà:</span>
        <span class="info-val">%SSID%</span>
      </div>
      <div class="info-row">
        <span class="info-label">IP Nội bộ (Local):</span>
        <span class="info-val" style="color: #38bdf8;">%IP%</span>
      </div>
      <div class="info-row">
        <span class="info-label">RAM trống (Heap):</span>
        <span class="info-val" id="heap-val" style="color: #4ade80;">%HEAP% KB</span>
      </div>
      <div class="info-row">
        <span class="info-label">Đã truyền tải:</span>
        <span class="info-val" id="bytes-val">%MB% MB</span>
      </div>
    </div>

    <a href="/update" class="btn">🚀 Nạp Firmware Mới Qua Wi-Fi (OTA)</a>
  </div>

  <script>
    setInterval(() => {
      fetch('/api/status')
        .then(r => r.json())
        .then(d => {
          if (d.temp) document.getElementById('temp-val').innerHTML = d.temp.toFixed(1) + ' &deg;C';
          if (d.sessions !== undefined) document.getElementById('sessions-val').innerText = d.sessions;
          if (d.heap) document.getElementById('heap-val').innerText = d.heap + ' KB';
          if (d.mb !== undefined) document.getElementById('bytes-val').innerText = d.mb + ' MB';
        })
        .catch(() => {});
    }, 2000);
  </script>
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
    body { background: #090d16; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #131b2e; border-radius: 20px; padding: 28px; width: 100%; max-width: 440px; box-shadow: 0 15px 35px rgba(0,0,0,0.6); border: 1px solid #1e293b; }
    h1 { font-size: 20px; text-align: center; margin-bottom: 20px; color: #38bdf8; }
    .upload-area { border: 2px dashed #475569; border-radius: 12px; padding: 24px; text-align: center; margin-bottom: 20px; background: #0b1120; }
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
      Serial.print("[SOCKS5] >>> PORT PROXY: ");
      Serial.println(SOCKS5_PORT);
      Serial.print("[OTA]    >>> TRANG NAP CODE: http://");
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
  html.replace("%HEAP%", String(ESP.getFreeHeap() / 1024));
  html.replace("%TEMP%", String(readChipTemp(), 1));
  html.replace("%SESSIONS%", String(activeSessions));
  html.replace("%USER%", PROXY_USER);
  html.replace("%PASS%", PROXY_PASS);
  html.replace("%MB%", String((float)totalBytesRelayed / (1024.0 * 1024.0), 2));
  server.send(200, "text/html; charset=utf-8", html);
}

void handleApiStatus() {
  String json = "{'temp':" + String(readChipTemp(), 1) +
                ",'sessions':" + String(activeSessions) +
                ",'heap':" + String(ESP.getFreeHeap() / 1024) +
                ",'rssi':" + String(WiFi.RSSI()) +
                ",'mb':" + String((float)totalBytesRelayed / (1024.0 * 1024.0), 2) +
                ",'ip':'" + localIP + "'}";
  json.replace(char(39), char(34));
  server.send(200, "application/json", json);
}

void handleUpdatePage() {
  server.send(200, "text/html; charset=utf-8", FPSTR(UPDATE_HTML));
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("");
  Serial.println("==================================================");
  Serial.println("=== ESP32 DEVKIT V1 - RESIDENTIAL SOCKS5 PROXY ===");
  Serial.println("=== Chip: ESP32 Dual-Core | Port: 1080 (Auth)  ===");
  Serial.println("==================================================");

  connectToWiFi();

  // Khoi dong SOCKS5 Server
  socksServer.begin();
  Serial.println("[SOCKS5] Server da lang nghe tren cong 1080!");

  // Web Server & OTA
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
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
  Serial.println("[System] Web Dashboard da khoi dong tren port 80!");
}

void loop() {
  server.handleClient();

  // Lang nghe cac ket noi SOCKS5 toi port 1080
  WiFiClient client = socksServer.available();
  if (client) {
    ProxySession* session = new ProxySession{client};
    // Tao task FreeRTOS de phuc vu ket noi nay tren Core 1
    xTaskCreatePinnedToCore(
      socksWorkerTask,
      "socks_worker",
      4096,
      session,
      1,
      NULL,
      1
    );
  }

  // Tu dong ket noi lai Wi-Fi neu rot
  if (WiFi.status() != WL_CONNECTED && millis() - lastHeartbeat > 10000) {
    connectToWiFi();
  }

  // Heartbeat moi 5 giay
  if (millis() - lastHeartbeat >= 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_PIN, HIGH);
    Serial.print("[Heartbeat] IP: ");
    Serial.print(localIP);
    Serial.print(" | SOCKS5: 1080 | Active: ");
    Serial.print(activeSessions);
    Serial.print(" | Free Heap: ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB | Blue LED: ON");
  }
}
