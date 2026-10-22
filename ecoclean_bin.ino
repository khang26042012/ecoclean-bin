/*
 * ==============================================================================
 * DỰ ÁN KHKT: THÙNG RÁC HỌC ĐƯỜNG THÔNG MINH KHÔNG CHẠM (ECOCLEAN BIN)
 * Tác giả: Anh Khang (KhangSMP / KhangKYT) & AI Assistant
 * Bo mạch: ESP32-C3 Super Mini (RISC-V 160MHz, 4MB Flash)
 * Thư viện yêu cầu: ESP32Servo (Tải từ Library Manager trên Arduino IDE)
 * ==============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <ArduinoOTA.h>

// --- CẤU HÌNH CHÂN KẾT NỐI (PINOUT) ---
#define SERVO_PIN        3   // Chân tín hiệu Servo SG90 (Dây cam)
#define TRIG_PIN         0   // Chân Trig cảm biến siêu âm HC-SR04
#define ECHO_PIN         1   // Chân Echo cảm biến siêu âm HC-SR04
#define LED_PIN          8   // LED tích hợp trên bo ESP32-C3 Super Mini (Active LOW)

// --- THÔNG SỐ VẬN HÀNH THÙNG RÁC ---
const int DISTANCE_THRESHOLD_CM = 20;   // Cự ly bàn tay kích hoạt mở nắp (< 20cm)
const int SERVO_OPEN_ANGLE      = 90;   // Góc mở nắp (90 độ)
const int SERVO_CLOSE_ANGLE     = 0;    // Góc đóng nắp (0 độ)
const unsigned long OPEN_TIME_MS = 5000; // Thời gian giữ mở nắp để bỏ rác (5 giây)

// --- MẬT KHẨU WI-FI ĐỊNH DANH ---
const char* WIFI_PASSWORD = "khongbiet123";

// --- KHỞI TẠO ĐỐI TƯỢNG ---
Servo myServo;
WebServer server(80);

// --- BIẾN TOÀN CỤC ---
bool isLidOpen = false;
unsigned long lidOpenedTime = 0;
unsigned long trashCount = 0;
float currentDistance = 0.0;
String connectedSSID = "Chưa kết nối";

// ==============================================================================
// HÀM ĐO KHOẢNG CÁCH SIÊU ÂM (HC-SR04)
// ==============================================================================
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Chờ xung phản hồi với timeout 30ms (~5 mét)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0; // Không có vật cản
  
  float distance = (duration * 0.0343) / 2.0;
  return distance;
}

// ==============================================================================
// CÁC HÀM ĐIỀU KHIỂN NẮP THÙNG RÁC
// ==============================================================================
void openLid() {
  if (!isLidOpen) {
    Serial.println("\n>>> [HÀNH ĐỘNG] PHÁT HIỆN BÀN TAY -> MỞ NẮP 90 ĐỘ!");
    myServo.write(SERVO_OPEN_ANGLE);
    digitalWrite(LED_PIN, LOW); // Bật LED báo hiệu
    isLidOpen = true;
    lidOpenedTime = millis();
    trashCount++;
  }
}

void closeLid() {
  if (isLidOpen) {
    Serial.println(">>> [HÀNH ĐỘNG] HẾT 5 GIÂY -> TỰ ĐỘNG ĐÓNG NẮP KÍN MÙI!");
    myServo.write(SERVO_CLOSE_ANGLE);
    digitalWrite(LED_PIN, HIGH); // Tắt LED
    isLidOpen = false;
    Serial.println("📢 [LOA] \"Cảm ơn bạn đã bỏ rác đúng nơi quy định!\"");
  }
}

// ==============================================================================
// HÀM TỰ ĐỘNG QUÉT & KẾT NỐI WI-FI VỚI PASS: khongbiet123
// ==============================================================================
void autoScanAndConnectWiFi() {
  Serial.println("\n==================================================");
  Serial.println("🔍 [WI-FI] Bắt đầu quét các mạng Wi-Fi xung quanh...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("❌ Không tìm thấy mạng Wi-Fi nào!");
  } else {
    Serial.printf("✅ Tìm thấy %d mạng Wi-Fi:\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("   %d: %s (%d dBm)%s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [Mở]" : "");
    }

    // Thử kết nối lần lượt các mạng có sóng mạnh nhất với mật khẩu khongbiet123
    for (int i = 0; i < n; ++i) {
      String targetSSID = WiFi.SSID(i);
      Serial.printf("\n🔄 Đang thử kết nối vào: '%s' với pass '%s'...\n", targetSSID.c_str(), WIFI_PASSWORD);
      WiFi.begin(targetSSID.c_str(), WIFI_PASSWORD);

      unsigned long startAttemptTime = millis();
      // Chờ tối đa 8 giây cho mỗi mạng
      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 8000) {
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        connectedSSID = targetSSID;
        Serial.println("\n🎉 KẾT NỐI WI-FI THÀNH CÔNG RỰC RỠ!");
        Serial.printf("🌐 Tên mạng: %s\n", connectedSSID.c_str());
        Serial.printf("📡 Địa chỉ IP: http://%s\n", WiFi.localIP().toString().c_str());
        Serial.println("==================================================");
        return;
      } else {
        Serial.println(" (Thất bại, thử mạng kế tiếp...)");
        WiFi.disconnect();
      }
    }
  }

  // Nếu thử hết các mạng không được, phát Access Point dự phòng
  Serial.println("\n⚠️ Không thể kết nối các mạng quét được. Đang mở Wi-Fi Access Point dự phòng...");
  WiFi.softAP("EcoClean-Bin-Setup", "12345678");
  Serial.printf("🌐 Wi-Fi phát: EcoClean-Bin-Setup (Pass: 12345678)\n");
  Serial.printf("📡 Địa chỉ IP: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("==================================================");
}

// ==============================================================================
// GIAO DIỆN WEB SERVER ĐIỀU KHIỂN TRỰC QUAN (PORT 80)
// ==============================================================================
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='vi'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>EcoClean Bin Dashboard</title>";
  html += "<style>";
  html += "body { font-family: sans-serif; background: #0f172a; color: #fff; text-align: center; padding: 20px; }";
  html += ".card { background: #1e293b; border-radius: 16px; padding: 20px; max-width: 450px; margin: auto; border: 2px solid #334155; }";
  html += "h1 { color: #38bdf8; font-size: 1.5rem; }";
  html += ".status { font-size: 1.2rem; font-weight: bold; margin: 15px 0; color: " + String(isLidOpen ? "#22c55e" : "#94a3b8") + "; }";
  html += ".btn { display: inline-block; padding: 12px 24px; margin: 8px; font-size: 1rem; font-weight: bold; border-radius: 12px; border: none; cursor: pointer; text-decoration: none; }";
  html += ".btn-open { background: #38bdf8; color: #000; }";
  html += ".btn-close { background: #ef4444; color: #fff; }";
  html += ".stat-box { background: #020617; padding: 12px; border-radius: 10px; margin-top: 15px; font-size: 0.9rem; color: #94a3b8; }";
  html += "</style></head><body>";
  html += "<div class='card'>";
  html += "<h1>🗑️ THÙNG RÁC ECOCLEAN</h1>";
  html += "<div class='status'>Trạng Thái: " + String(isLidOpen ? "🔓 ĐANG MỞ" : "🔒 ĐÃ ĐÓNG KÍN") + "</div>";
  html += "<div><a href='/open' class='btn btn-open'>Mở Nắp</a><a href='/close' class='btn btn-close'>Đóng Nắp</a></div>";
  html += "<div class='stat-box'>";
  html += "<p>📏 Khoảng cách cảm biến: <strong>" + String(currentDistance, 1) + " cm</strong></p>";
  html += "<p>📊 Tổng lượt bỏ rác: <strong>" + String(trashCount) + " lần</strong></p>";
  html += "<p>📶 Mạng Wi-Fi: <strong>" + connectedSSID + "</strong></p>";
  html += "</div>";
  html += "</div>";
  html += "<script>setTimeout(() => { location.reload(); }, 3000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleOpen() {
  openLid();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClose() {
  closeLid();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ==============================================================================
// XỬ LÝ LỆNH TỪ SERIAL MONITOR (WEB CHROME GỬI XUỐNG)
// ==============================================================================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "OPEN") {
      openLid();
    } else if (cmd == "CLOSE") {
      closeLid();
    } else if (cmd == "SCAN") {
      autoScanAndConnectWiFi();
    } else if (cmd == "IP") {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("📡 IP: http://%s (Mạng: %s)\n", WiFi.localIP().toString().c_str(), connectedSSID.c_str());
      } else {
        Serial.printf("📡 AP IP: http://%s\n", WiFi.softAPIP().toString().c_str());
      }
    } else if (cmd == "STATUS") {
      Serial.printf("📊 Nắp: %s | Khoảng cách: %.1f cm | Đã bỏ rác: %lu lần\n", isLidOpen ? "MỞ" : "ĐÓNG", currentDistance, trashCount);
    }
  }
}

// ==============================================================================
// SETUP: KHỞI TẠO HỆ THỐNG
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n==================================================");
  Serial.println("🚀 [ECOCLEAN BIN] KHỞI ĐỘNG HỆ THỐNG THÙNG RÁC THÔNG MINH");
  Serial.println("==================================================");

  // Cấu hình chân GPIO
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Tắt LED (Active Low)

  // Cấu hình Servo SG90
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50); // Servo SG90 tiêu chuẩn 50Hz
  myServo.attach(SERVO_PIN, 500, 2400); // 500us - 2400us
  myServo.write(SERVO_CLOSE_ANGLE); // Ban đầu đóng nắp chặt 0 độ

  // Quét & Kết nối Wi-Fi tự động
  autoScanAndConnectWiFi();

  // Khởi động ArduinoOTA (Cập nhật không dây)
  ArduinoOTA.setHostname("ecoclean-bin");
  ArduinoOTA.begin();

  // Cấu hình Web Server
  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.begin();
  Serial.println("🌍 Web Server đã sẵn sàng phục vụ trên cổng 80!");
  Serial.println("==================================================");
}

// ==============================================================================
// LOOP: VÒNG LẶP CHÍNH
// ==============================================================================
void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  handleSerialCommands();

  // Đo khoảng cách mỗi 150ms
  static unsigned long lastMeasure = 0;
  if (millis() - lastMeasure > 150) {
    lastMeasure = millis();
    currentDistance = measureDistance();

    // Nếu phát hiện tay dưới 20cm và nắp chưa mở
    if (currentDistance > 1.5 && currentDistance <= DISTANCE_THRESHOLD_CM) {
      openLid();
    }
  }

  // Tự động đóng nắp sau 5 giây
  if (isLidOpen && (millis() - lidOpenedTime >= OPEN_TIME_MS)) {
    closeLid();
  }
}
