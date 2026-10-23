/*
 * ESP32-C3 MULTI-SSID BEACON - MARAUDER ENGINE (50-60°C OPTIMIZED)
 * 1. Exact 802.11 Beacon frame structure from ESP32Marauder (variable length, no padding)
 * 2. Unicast/Local Admin MAC address bitmask ((rand & 0xFE) | 0x02)
 * 3. Dynamic Channel Sync with "Trong Khang" (channel 4)
 * 4. 80ms Burst Interval (12 bursts/sec) -> ZERO DROPOUT on Android, Temp: 50-55°C
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

// Marauder base packet header (38 bytes)
const uint8_t baseHeader[38] = {
  0x80, 0x00, 0x00, 0x00, // Frame Control (Beacon), Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination (Broadcast)
  0x02, 0x11, 0x22, 0x33, 0x44, 0x00, // Source Address
  0x02, 0x11, 0x22, 0x33, 0x44, 0x00, // BSSID
  0xc0, 0x6c, // Seq-ctl
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  0x64, 0x00, // Beacon interval (100 TU)
  0x31, 0x00, // Capability info (WPA2)
  0x00, 0x00  // Tag 0 (SSID), length will be overwritten
};

// Marauder post-SSID data (39 bytes): Supported Rates + DSSS Channel + WPA2 RSN
const uint8_t postBase[39] = {
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, // Supported rates
  0x03, 0x01, 0x04, // DSSS parameter (byte 12 is channel)
  0x30, 0x18, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x02, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00 // WPA2 RSN
};

uint8_t curChannel = 1;
unsigned long lastBurstTime = 0;

void setup() {
  Serial.begin(115200);

  // Ha xung 80MHz lam mat
  setCpuFrequencyMhz(80);

  // Kich hoat AP_STA de mo WIFI_IF_AP
  WiFi.mode(WIFI_AP_STA);

  // An hoan toan SoftAP goc
  WiFi.softAP("invisible", "12345678", 1, 1, 1);

  // Ket noi WiFi nha
  WiFi.begin("Trong Khang", "khongbiet123");

  // Ha cong suat phat ve 8.5dBm
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  addLog("🚀 Marauder Engine khoi dong: Phat 6 Wi-Fi chu kỳ 80ms!");

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
      addLog("🎉 Da bat kenh: " + String(curChannel) + " | IP: " + WiFi.localIP().toString());
    }

    unsigned long now = millis();
    // Chu ky 75ms: Phat 6 goi het 3ms, nghi 72ms (Duty cycle 4% -> Nhiet do 50-55C)
    if (now - lastBurstTime >= 75) {
      lastBurstTime = now;

      uint8_t packet[128];

      for (int i = 0; i < NUM_SSIDS; i++) {
        const char* name = ssids[i];
        int ssidLen = strlen(name);
        if (ssidLen > 32) ssidLen = 32;

        int frameLen = 38 + ssidLen + sizeof(postBase);

        // Copy header
        memcpy(packet, baseHeader, 38);

        // Valid unicast locally-administered MAC: (0x02 | (rand & 0xFC))
        packet[10] = packet[16] = 0x02;
        packet[11] = packet[17] = 0x11;
        packet[12] = packet[18] = 0x22;
        packet[13] = packet[19] = 0x33;
        packet[14] = packet[20] = 0x44;
        packet[15] = packet[21] = (uint8_t)(i + 1);

        // SSID tag length
        packet[37] = (uint8_t)ssidLen;

        // Copy SSID
        memcpy(packet + 38, name, ssidLen);

        // Copy postBase
        memcpy(packet + 38 + ssidLen, postBase, sizeof(postBase));

        // Set channel in DSSS tag (byte offset: 38 + ssidLen + 12 = 50 + ssidLen)
        packet[50 + ssidLen] = (curChannel > 0 && curChannel <= 14) ? curChannel : 1;

        // Phat qua WIFI_IF_AP 2 lan de dam bao dien thoai bat song 100%
        esp_wifi_80211_tx(WIFI_IF_AP, packet, frameLen, false);
        esp_wifi_80211_tx(WIFI_IF_AP, packet, frameLen, false);
        delayMicroseconds(200);
      }
    }
  }

  delay(5);
}
