/*
 * ESP32-C3 MULTI-SSID BEACON - TROLL EDITION (HIDDEN AP + HIGH COMPATIBILITY)
 * 1. WiFi.softAP("invisible", ..., 1, 1) -> Ẩn vĩnh viễn SoftAP (100% không còn ESP32-SERVO)
 * 2. WIFI_IF_AP active -> Bắn 6 mạng Wi-Fi troll rõ ràng, điện thoại quét thấy ngay lập tức!
 * 3. TxPower 5dBm, Burst 120ms -> Mát rượi (48-52°C)
 * 4. Giữ kết nối STA Trong Khang & Web OTA (/update, /temp, /log)
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

// 6 Mang Wi-Fi troll sap xep theo thu tu
const char* ssids[] = {
  "01. Tao co wifi",
  "02. Tao cung co",
  "03. Ai hoi chung may vay",
  "04. Chac tao khong co",
  "05. Mu het a",
  "06. Co caizai, tao manh nhat"
};
const int NUM_SSIDS = sizeof(ssids) / sizeof(ssids[0]);

uint8_t beaconPacket[109] = {
  /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00,
  /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  /* 10 - 15 */ 0x02, 0x11, 0x22, 0x33, 0x44, 0x00,
  /* 16 - 21 */ 0x02, 0x11, 0x22, 0x33, 0x44, 0x00,

  /* 22 - 23 */ 0x00, 0x00,
  /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
  /* 32 - 33 */ 0x64, 0x00,
  /* 34 - 35 */ 0x31, 0x04,

  /* 36 - 37 */ 0x00, 0x20,
  /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

  /* 70 - 71 */ 0x01, 0x08,
  /* 72 - 79 */ 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,

  /* 80 - 81 */ 0x03, 0x01,
  /* 82 */      0x01,

  /* 83 - 84 */ 0x30, 0x18,
  /* 85 - 86 */ 0x01, 0x00,
  /* 87 - 90 */ 0x00, 0x0f, 0xac, 0x02,
  /* 91 - 92 */ 0x02, 0x00,
  /* 93 - 100*/ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
  /* 101-102 */ 0x01, 0x00,
  /* 103-106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107-108 */ 0x00, 0x00
};

uint8_t curChannel = 1;
unsigned long lastBurstTime = 0;

void setup() {
  Serial.begin(115200);

  // Ha xung 80MHz lam mat
  setCpuFrequencyMhz(80);

  // Kich hoat AP_STA de mo kenh WIFI_IF_AP cho ham esp_wifi_80211_tx
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(true);

  // 🔥 ẨN HOÀN TOÀN MẠNG GỐC: ssid_hidden = 1, tên "invisible" -> 100% không hiện chữ ESP32 hay SERVO!
  WiFi.softAP("invisible", "12345678", 1, 1, 1);

  // Ket noi WiFi nha
  WiFi.begin("Trong Khang", "khongbiet123");

  // Cong suat 5dBm (chi 3mW) vua tiet kiem dien vua mat ruoi
  WiFi.setTxPower(WIFI_POWER_5dBm);

  addLog("🚀 ĐÃ KÍCH HOẠT: Ẩn SoftAP gốc + Bắn 6 Wi-Fi Troll qua WIFI_IF_AP!");

  // Endpoints OTA & Giam sat
  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n🌡️ NHIET DO CHIP: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do: " + String(temp, 1) + " C");
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

  static bool wifiAnnounced = false;
  if (!wifiAnnounced && WiFi.status() == WL_CONNECTED) {
    wifiAnnounced = true;
    curChannel = WiFi.channel();
    addLog("🎉 Wi-Fi OK! IP: " + WiFi.localIP().toString() + " (Kênh: " + String(curChannel) + ")");
  }

  // Chu ky 120ms: Ban 6 goi tin qua WIFI_IF_AP (mat 3ms), 117ms nghi ngoi
  unsigned long now = millis();
  if (now - lastBurstTime >= 120) {
    lastBurstTime = now;

    beaconPacket[82] = (curChannel > 0 && curChannel <= 14) ? curChannel : 1;

    for (int i = 0; i < NUM_SSIDS; i++) {
      const char* name = ssids[i];
      int nameLen = strlen(name);
      if (nameLen > 32) nameLen = 32;

      beaconPacket[10] = 0x02;
      beaconPacket[15] = (uint8_t)(i + 1);
      beaconPacket[16] = 0x02;
      beaconPacket[21] = (uint8_t)(i + 1);

      memset(&beaconPacket[38], ' ', 32);
      memcpy(&beaconPacket[38], name, nameLen);

      // Bắn qua WIFI_IF_AP (chuan 802.11 cho Beacon)
      esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
      delayMicroseconds(500);
    }
  }

  delay(10);
}
