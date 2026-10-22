/*
 * ESP32-C3 WIFI OTA FIRMWARE - DUAL MODE (AP + STA)
 * WiFi Home: Trong Khang (Pass: khongbiet123)
 * WiFi AP: ESP32-C3-OTA (Pass: 12345678, IP: 192.168.4.1)
 */
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

const char* TARGET_PASS = "khongbiet123";
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. KICH HOAT CA 2 CHE DO: VUA PHAT SÓNG (AP) VUA BAT WI-FI NHA (STA)
  WiFi.mode(WIFI_AP_STA);

  // Phat song AP ngay lap tuc de dien thoai luon nhin thay
  WiFi.softAP("ESP32-C3-OTA", "12345678");
  Serial.println("\n==================================================");
  Serial.println("📡 DA PHAT SONG WI-FI: ESP32-C3-OTA (Pass: 12345678)");
  Serial.printf("🌐 IP TRUY CAP TRUC TIEP: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("==================================================");

  // 2. KET NOI WI-FI TRONG KHANG / SCAN
  Serial.println("🔍 Dang tim ket noi vao Wi-Fi Trong Khang...");
  WiFi.begin("Trong Khang", TARGET_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n🎉 VAO DUOC WI-FI 'Trong Khang' THANH CONG!");
    Serial.printf("📡 IP MANG NHA: http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️ Khong vao duoc 'Trong Khang', dang thu scan cac mang khac...");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid == "Trong Khang") continue;
      Serial.printf("Thu '%s'...\n", ssid.c_str());
      WiFi.begin(ssid.c_str(), TARGET_PASS);
      unsigned long s = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - s < 6000) {
        delay(400);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n🎉 KET NOI THANH CONG: %s | IP: http://%s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
        break;
      }
    }
  }

  // 3. KHOI DONG ARDUINO OTA (NAP CODE QUA WIFI)
  ArduinoOTA.setHostname("esp32c3-bin");
  ArduinoOTA.begin();
  Serial.println("⚡ ARDUINO OTA DA SAN SANG!");

  // 4. TRANG WEB DASHBOARD
  server.on("/", []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP32-C3 OTA Dashboard</title>";
    html += "<style>body{font-family:sans-serif;background:#0f172a;color:#fff;text-align:center;padding:20px;}";
    html += ".card{background:#1e293b;border-radius:16px;padding:24px;max-width:400px;margin:auto;border:2px solid #38bdf8;}";
    html += "h1{color:#38bdf8;font-size:1.4rem;} p{margin:10px 0;color:#94a3b8;}</style></head><body>";
    html += "<div class='card'><h1>⚡ ESP32-C3 OTA SAN SANG!</h1>";
    html += "<p>Trang thai: <strong style='color:#22c55e;'>HOAT DONG 100%</strong></p>";
    html += "<p>IP mang AP: <strong>" + WiFi.softAPIP().toString() + "</strong></p>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "<p>IP mang nha: <strong>" + WiFi.localIP().toString() + "</strong> (" + WiFi.SSID() + ")</p>";
    }
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
