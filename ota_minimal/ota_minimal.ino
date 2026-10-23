/*
 * ESP32-C3 MULTI-SSID BEACON - STANDALONE OFFLINE EDITION
 * 1. SoftAP: "01. Tao co wifi", password: "26042012khang", Channel: 1
 * 2. 5 Beacons: "02. Tao cung co" -> "06. Co caizai, tao manh nhat" (Marauder format)
 * 3. Web UI: http://192.168.4.1/ (Xem nhiet do chip), /temp, /log, /update
 * 4. TxPower 5dBm, 140ms burst, CPU 80MHz -> Chay mat ruoi khong can tan nhiet
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "esp_wifi.h"

WebServer server(80);
String logs = "";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2000) logs = logs.substring(logs.indexOf('\n') + 1);
}

const char* ssids[] = {
  "01. Tao co wifi",
  "02. Tao cung co",
  "03. Ai hoi chung may vay",
  "04. Chac tao khong co",
  "05. Mu het a",
  "06. Co caizai, tao manh nhat"
};
const int NUM_SSIDS = sizeof(ssids) / sizeof(ssids[0]);

const uint8_t baseHeader[38] = {
  0x80, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x02, 0x11, 0x22, 0x33, 0x44, 0x00,
  0x02, 0x11, 0x22, 0x33, 0x44, 0x00,
  0xc0, 0x6c,
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
  0x64, 0x00,
  0x31, 0x00,
  0x00, 0x00
};

const uint8_t postBase[39] = {
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,
  0x03, 0x01, 0x01,
  0x30, 0x18, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x02, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
};

const uint8_t curChannel = 1;
unsigned long lastBurstTime = 0;

void setup() {
  Serial.begin(115200);

  // Ha xung 80MHz lam mat
  setCpuFrequencyMhz(80);

  // Che do thuan SoftAP doc lap hoan toan (khong ton dien quet tim mang nha)
  WiFi.mode(WIFI_AP);

  // Khoi tao mang goc tren Kenh 1, mat khau: 26042012khang
  WiFi.softAP("01. Tao co wifi", "26042012khang", curChannel);

  // Ha cong suat phat song ve 5dBm (chi ~3mW) de chip mat ruoi
  WiFi.setTxPower(WIFI_POWER_5dBm);

  addLog("❄️ OFFLINE STANDALONE: SoftAP 01 (Pass: 26042012khang) + 5 Beacons! IP: 192.168.4.1");

  // Giao dien xem nhiet do dep mat tren dien thoai
  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>ESP32 Nhiet Do</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:30px;background:#121212;color:#fff;}";
    html += ".card{background:#1e1e1e;border-radius:16px;padding:25px;max-width:350px;margin:0 auto;box-shadow:0 8px 24px rgba(0,0,0,0.5);}";
    html += ".temp{font-size:48px;font-weight:bold;color:#4caf50;margin:15px 0;}";
    html += "</style></head><body>";
    html += "<div class='card'>";
    html += "<h2>ESP32 STANDALONE</h2>";
    html += "<p>Nhiệt độ lõi chip:</p>";
    html += "<div class='temp'>" + String(temp, 1) + " &deg;C</div>";
    html += "<p style='color:#aaa;'>Trạng thái: Mát mẻ & Độc lập</p>";
    html += "<p><a href='/log' style='color:#64b5f6;'>Xem Nhật Ký (Log)</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do chip: " + String(temp, 1) + " C");
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n🌡️ NHIET DO CHIP: " + String(temp, 1) + " C";
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

  unsigned long now = millis();
  // Chu ky 140ms: Phat 5 mang het 1.5ms, nghi 138.5ms (Duty cycle ~1% -> Mat ruoi 45-50C)
  if (now - lastBurstTime >= 140) {
    lastBurstTime = now;

    uint8_t packet[128];

    for (int i = 1; i < NUM_SSIDS; i++) {
      const char* name = ssids[i];
      int ssidLen = strlen(name);
      if (ssidLen > 32) ssidLen = 32;

      int frameLen = 38 + ssidLen + sizeof(postBase);

      memcpy(packet, baseHeader, 38);

      packet[10] = packet[16] = 0x02;
      packet[11] = packet[17] = 0x11;
      packet[12] = packet[18] = 0x22;
      packet[13] = packet[19] = 0x33;
      packet[14] = packet[20] = 0x44;
      packet[15] = packet[21] = (uint8_t)(i + 1);

      packet[37] = (uint8_t)ssidLen;
      memcpy(packet + 38, name, ssidLen);
      memcpy(packet + 38 + ssidLen, postBase, sizeof(postBase));
      packet[50 + ssidLen] = curChannel;

      esp_wifi_80211_tx(WIFI_IF_AP, packet, frameLen, false);
      delayMicroseconds(250);
    }
  }

  delay(10);
}
