/*
 * ESP32-C3 MULTI-SSID BEACON - PERFECT MARAUDER (50-60°C TUNED)
 * 1. Marauder Dynamic Frame: Chuan 100% Android nhan dien ca 6 mang!
 * 2. 1 goi/mang moi 135ms (Duty cycle 1%) -> Nhiet do on dinh 50 - 55°C
 * 3. TxPower 5dBm (chi 3mW) -> Khong bao gio qua nhiet
 * 4. Khong sleep qua lau (chi 135ms) -> Khong bao gio bi mat song tren dien thoai
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
  0x03, 0x01, 0x04,
  0x30, 0x18, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x02, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
};

uint8_t curChannel = 1;
unsigned long lastBurstTime = 0;

void setup() {
  Serial.begin(115200);

  // Ha xung 80MHz lam mat
  setCpuFrequencyMhz(80);

  // Kich hoat AP_STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("invisible", "12345678", 1, 1, 1);
  WiFi.begin("Trong Khang", "khongbiet123");

  // Ha cong suat phat song ve 5dBm de giu nhiet do 50-60C
  WiFi.setTxPower(WIFI_POWER_5dBm);

  addLog("❄️ PERFECT MARAUDER: TxPower 5dBm, Burst 135ms, Target Temp: 50-55C");

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

  if (WiFi.status() == WL_CONNECTED) {
    static bool channelSynced = false;
    if (!channelSynced) {
      channelSynced = true;
      curChannel = WiFi.channel();
      addLog("🎉 Kenh: " + String(curChannel) + " | IP: " + WiFi.localIP().toString());
    }

    unsigned long now = millis();
    // Chu ky 135ms (chi 0.13s): Khong he sleep qua lau, ban 6 goi het 1.5ms, nghi 133.5ms
    if (now - lastBurstTime >= 135) {
      lastBurstTime = now;

      uint8_t packet[128];

      for (int i = 0; i < NUM_SSIDS; i++) {
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
        packet[50 + ssidLen] = (curChannel > 0 && curChannel <= 14) ? curChannel : 1;

        // Phat 1 goi chuan/mang
        esp_wifi_80211_tx(WIFI_IF_AP, packet, frameLen, false);
        delayMicroseconds(250);
      }
    }
  }

  delay(10);
}
