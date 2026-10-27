/*
 * ESP32-C3 SUPER MINI - TEST GPIO 3 & GPIO 4 (PINS 7 & 8)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
long currentDistance = -1;
String pinPair = "Chưa xác định";

long measureWithPins(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 30000);
  if (duration <= 0) return -1;
  return (long)(duration * 0.034 / 2);
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

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDistance));
  });

  server.on("/raw", HTTP_GET, []() {
    String s = "Dist=" + String(currentDistance) + " cm | Config: " + pinPair;
    s += " | GPIO0=" + String(digitalRead(0)) + " GPIO1=" + String(digitalRead(1)) + " GPIO3=" + String(digitalRead(3)) + " GPIO4=" + String(digitalRead(4));
    server.send(200, "text/plain", s);
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

  static unsigned long lastMeasure = 0;
  unsigned long now = millis();
  if (now - lastMeasure >= 200) {
    lastMeasure = now;

    // Test Thu 1: Trig=3, Echo=4
    long d = measureWithPins(3, 4);
    if (d >= 0 && d <= 400) {
      currentDistance = d;
      pinPair = "Trig=GPIO3 (Lỗ 7), Echo=GPIO4 (Lỗ 8)";
    } else {
      // Test Thu 2: Trig=4, Echo=3
      d = measureWithPins(4, 3);
      if (d >= 0 && d <= 400) {
        currentDistance = d;
        pinPair = "Trig=GPIO4 (Lỗ 8), Echo=GPIO3 (Lỗ 7)";
      } else {
        // Test Thu 3: Trig=0, Echo=1 (nếu cắm lỗ 4 & 5)
        d = measureWithPins(0, 1);
        if (d >= 0 && d <= 400) {
          currentDistance = d;
          pinPair = "Trig=GPIO0 (Lỗ 4), Echo=GPIO1 (Lỗ 5)";
        } else {
          d = measureWithPins(1, 0);
          if (d >= 0 && d <= 400) {
            currentDistance = d;
            pinPair = "Trig=GPIO1 (Lỗ 5), Echo=GPIO0 (Lỗ 4)";
          } else {
            currentDistance = -1;
          }
        }
      }
    }
  }

  delay(5);
}
