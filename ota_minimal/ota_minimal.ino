/*
 * ESP32-C3 MULTI-SSID BEACON - ULTRA COOLING OPTIMIZATION
 * 1. WiFi.setTxPower(WIFI_POWER_8_5dBm) -> Cut RF current by 60%
 * 2. Burst TX (4ms) + 120ms Sleep Window -> 96% PA Off-duty
 * 3. CPU 80MHz + OTA Web Server preserved
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
  "01. while(em_con_tho)",
  "02. {",
  "03.    Em_Yeu_Chi();",
  "04.    Nhieu_Lam();",
  "05. }",
  "06. return 100_Diem;",
  "07. // Mai ben chi <3"
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

  // Ha xung 80MHz de lam mat CPU
  setCpuFrequencyMhz(80);

  // Khoi tao WiFi AP+STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin("Trong Khang", "khongbiet123");

  // Giam cong suat phat song Wi-Fi tu 20dBm xuong 8.5dBm (giam 60% nhiet do va dong dien)
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  addLog("❄️ DA KÍCH HOẠT CHẾ ĐỘ LÀM MÁT TỐI ƯU (TxPower 8.5dBm, Burst 120ms)!");

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

  // Chu ky phat Burst: Moi 140ms ban 1 loat 7 mang (mat ~4ms), 136ms con lai chip nghi ngoi hoan toan
  unsigned long now = millis();
  if (now - lastBurstTime >= 140) {
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

      esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
      delayMicroseconds(600); // Gian cach nhe 0.6ms giua cac goi
    }
  }

  // Cho FreeRTOS nghi ngoi giup CPU va RF PA ha nhiet
  delay(15);
}
