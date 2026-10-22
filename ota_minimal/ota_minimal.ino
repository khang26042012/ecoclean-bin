/*
 * ESP32-C3 MINIMAL WIFI OTA FIRMWARE
 * Pass: khongbiet123
 */
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

const char* WIFI_PASSWORD = "khongbiet123";
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("🚀 ESP32-C3 DANG KHOI DONG...");
  Serial.println("=================================");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("🔍 Dang quet Wi-Fi...");
  int n = WiFi.scanNetworks();
  bool connected = false;

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    Serial.printf("🔄 Thu ket noi: '%s' voi pass '%s'...\n", ssid.c_str(), WIFI_PASSWORD);
    WiFi.begin(ssid.c_str(), WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(400);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n🎉 KET NOI THANH CONG: %s\n", ssid.c_str());
      Serial.printf("📡 IP: http://%s\n", WiFi.localIP().toString().c_str());
      connected = true;
      break;
    } else {
      Serial.println(" (That bai)");
      WiFi.disconnect();
    }
  }

  if (!connected) {
    WiFi.softAP("ESP32-C3-OTA", "12345678");
    Serial.printf("\n⚠️ Khong vao duoc Wi-Fi, mo AP: ESP32-C3-OTA (Pass: 12345678)\n");
    Serial.printf("📡 IP: http://%s\n", WiFi.softAPIP().toString().c_str());
  }

  ArduinoOTA.setHostname("esp32c3-bin");
  ArduinoOTA.begin();
  Serial.println("⚡ OTA SAN SANG! CO THE NAP CODE QUA WIFI NGAY BAY GIO!");

  server.on("/", []() {
    String s = "<h1>ESP32-C3 OTA SAN SANG!</h1>";
    s += "<p>IP: " + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "</p>";
    server.send(200, "text/html", s);
  });
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
