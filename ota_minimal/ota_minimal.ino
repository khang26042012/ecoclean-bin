/*
 * ESP32-C3 MULTI-SSID BEACON BROADCASTER + WEB OTA
 * Style: Programming / C++ Love Confession
 * Preserves STA connection to "Trong Khang" for Web OTA (/update, /temp, /log)
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

// Danh sách các mạng Wi-Fi tỏ tình (Style C++)
const char* ssids[] = {
  "01. while(em_con_tho)",
  "02. {",
  "03.    Em_Yeu_Chi();",
  "04.    Nhieu_Lam();",
  "05. }",
  "06. return 100_Diem;",
  "07. // Mai ben chi <3"
};
const int NUM_SSIDS = sizeof(ssids) / sizeof(ssids[0]);

// Khung gói tin 802.11 Beacon Frame (WPA2 có khóa bảo mật)
uint8_t beaconPacket[109] = {
  /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00, // Management frame - Beacon
  /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
  /* 10 - 15 */ 0x02, 0x11, 0x22, 0x33, 0x44, 0x00, // Source MAC (se thay doi theo SSID)
  /* 16 - 21 */ 0x02, 0x11, 0x22, 0x33, 0x44, 0x00, // BSSID

  // Fixed parameters
  /* 22 - 23 */ 0x00, 0x00, // Sequence control
  /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  /* 32 - 33 */ 0x64, 0x00, // Beacon Interval: 100 TU (~102.4ms)
  /* 34 - 35 */ 0x31, 0x04, // Capabilities: ESS + Privacy (WPA2)

  // SSID parameter (Tag 0, len 32)
  /* 36 - 37 */ 0x00, 0x20,
  /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

  // Supported Rates (Tag 1, len 8)
  /* 70 - 71 */ 0x01, 0x08,
  /* 72 - 79 */ 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,

  // Current Channel (Tag 3, len 1)
  /* 80 - 81 */ 0x03, 0x01,
  /* 82 */      0x01,

  // RSN IE / WPA2 (Tag 48, len 24)
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
unsigned long lastBeaconTime = 0;
int currentSSIDIdx = 0;

void setup() {
  Serial.begin(115200);

  // Giữ xung 80MHz mát mẻ
  setCpuFrequencyMhz(80);

  // Kết nối Wi-Fi nhà để giữ Web OTA
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin("Trong Khang", "khongbiet123");
  addLog("🚀 Đang khởi động trạm phát 7 Wi-Fi Lập Trình & Kết nối Trong Khang...");

  // Endpoints OTA & Giám sát
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

  // Phát luân phiên các mạng Wi-Fi tỏ tình mỗi 15ms
  unsigned long now = millis();
  if (now - lastBeaconTime >= 15) {
    lastBeaconTime = now;

    const char* name = ssids[currentSSIDIdx];
    int nameLen = strlen(name);
    if (nameLen > 32) nameLen = 32;

    // Gán địa chỉ MAC ảo riêng cho từng mạng để điện thoại phân biệt
    beaconPacket[10] = 0x02;
    beaconPacket[15] = (uint8_t)(currentSSIDIdx + 1);
    beaconPacket[16] = 0x02;
    beaconPacket[21] = (uint8_t)(currentSSIDIdx + 1);

    // Xóa trắng khoảng đệm SSID (dài 32 byte)
    memset(&beaconPacket[38], ' ', 32);
    // Điền tên mạng vào
    memcpy(&beaconPacket[38], name, nameLen);

    // Đặt kênh
    beaconPacket[82] = (curChannel > 0 && curChannel <= 14) ? curChannel : 1;

    // Bắn gói tin Beacon ra không gian!
    esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);

    currentSSIDIdx = (currentSSIDIdx + 1) % NUM_SSIDS;
  }

  delay(2);
}
