/*
 * ECO-CLEAN SENSOR - DEEP DIAGNOSTIC ON (0,1) AND (20,21)
 * High-voltage pulse (25us), 50ms Echo timeout, 70ms acoustic quiet time
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long dur_0_1 = 0;
long dur_1_0 = 0;
long dur_20_21 = 0;
long dur_21_20 = 0;

void addLog(String s) {
  logs += s + "\n";
  if (logs.length() > 3000) logs = logs.substring(logs.indexOf('\n') + 1);
}

long pingPair(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  digitalWrite(trig, LOW);
  delayMicroseconds(5);
  digitalWrite(trig, HIGH);
  delayMicroseconds(25); // Xung 25us dam bao clone HC-SR04 bat duoc 100%
  digitalWrite(trig, LOW);

  // Timeout 50ms (50000us) de khong bao gio bi cat mat xung
  long dur = pulseIn(echo, HIGH, 50000);
  return dur;
}

void setup() {
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin("Trong Khang", "khongbiet123");

  while (WiFi.status() != WL_CONNECTED) {
    delay(150);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW); // Sang LED on dinh

  server.on("/diag", HTTP_GET, []() {
    String s = "=== KẾT QUẢ ĐO XUNG THỰC TẾ TRÊN CẢ 4 CẤU HÌNH ===\n";
    s += "1. Trig=0,  Echo=1  -> Dur=" + String(dur_0_1) + " us" + (dur_0_1 > 0 ? " [KHOANG CACH: " + String((long)(dur_0_1 * 0.034 / 2)) + " cm]" : "") + "\n";
    s += "2. Trig=1,  Echo=0  -> Dur=" + String(dur_1_0) + " us" + (dur_1_0 > 0 ? " [KHOANG CACH: " + String((long)(dur_1_0 * 0.034 / 2)) + " cm]" : "") + "\n";
    s += "3. Trig=20, Echo=21 -> Dur=" + String(dur_20_21) + " us" + (dur_20_21 > 0 ? " [KHOANG CACH: " + String((long)(dur_20_21 * 0.034 / 2)) + " cm]" : "") + "\n";
    s += "4. Trig=21, Echo=20 -> Dur=" + String(dur_21_20) + " us" + (dur_21_20 > 0 ? " [KHOANG CACH: " + String((long)(dur_21_20 * 0.034 / 2)) + " cm]" : "") + "\n";
    s += "Pin states: P0=" + String(digitalRead(0)) + " P1=" + String(digitalRead(1)) + " P20=" + String(digitalRead(20)) + " P21=" + String(digitalRead(21)) + "\n";
    server.send(200, "text/plain; charset=utf-8", s);
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
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  }

  static unsigned long lastRun = 0;
  unsigned long now = millis();
  if (now - lastRun >= 300) {
    lastRun = now;

    // Do lan luot 4 cap voi thoi gian nghi 70ms giua cac lan de tranh vang am
    dur_0_1 = pingPair(0, 1);
    delay(70);
    dur_1_0 = pingPair(1, 0);
    delay(70);
    dur_20_21 = pingPair(20, 21);
    delay(70);
    dur_21_20 = pingPair(21, 20);
  }

  delay(5);
}
