/*
 * ESP32-C3 SUPER MINI - PULSE HUNTER & DIAGNOSTIC
 * Scans GPIO 0, 1, 2, 4, 5 to find which pin receives Echo pulse!
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String diagResult = "Chưa chạy test";
long currentDistance = -1;

String testPair(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT_PULLDOWN);
  digitalWrite(trig, LOW);
  delayMicroseconds(5);
  digitalWrite(trig, HIGH);
  delayMicroseconds(12);
  digitalWrite(trig, LOW);

  // Doc nhanh xem co bat ky xung HIGH nao khong trong 15000us
  unsigned long start = micros();
  long highCount = 0;
  long firstHighUs = -1;
  while (micros() - start < 15000) {
    if (digitalRead(echo) == HIGH) {
      highCount++;
      if (firstHighUs < 0) firstHighUs = micros() - start;
    }
  }

  if (highCount > 0) {
    long dist = (firstHighUs * 0.034) / 2;
    return "🔥 PHÁT HIỆN XUNG! Trig=" + String(trig) + " Echo=" + String(echo) + " -> Samples=" + String(highCount) + " | Delay=" + String(firstHighUs) + "us (~" + String(dist) + "cm)";
  }
  return "Trig=" + String(trig) + " Echo=" + String(echo) + " -> 0 xung";
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW);

  server.on("/scan", HTTP_GET, []() {
    String out = "=== KẾT QUẢ QUÉT TẤT CẢ CÁC CẶP CHÂN ===\n";
    // Test cac cap co the: (0,1), (1,0), (0,2), (2,0), (1,2), (2,1), (0,4), (4,0)
    int testPins[] = {0, 1, 2, 4};
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (i != j) {
          out += testPair(testPins[i], testPins[j]) + "\n";
        }
      }
    }
    diagResult = out;
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "GPIO0=" + String(digitalRead(0)) + " | GPIO1=" + String(digitalRead(1)) + " | GPIO2=" + String(digitalRead(2)) + " | GPIO4=" + String(digitalRead(4));
    server.send(200, "text/plain", s);
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String html = "<html><head><meta charset='utf-8'><title>Pulse Hunter</title></head><body>";
    html += "<h2>Pulse Hunter Diagnostic</h2>";
    html += "<p><a href='/scan'>Bấm vào đây để Quét tìm xung cảm biến (/scan)</a></p>";
    html += "<p><a href='/raw'>Xem trạng thái các chân (/raw)</a></p>";
    html += "<pre>" + diagResult + "</pre>";
    html += "</body></html>";
    server.send(200, "text/html; charset=utf-8", html);
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
  delay(10);
}
