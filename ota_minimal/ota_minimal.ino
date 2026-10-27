/*
 * ESP32-C3 SUPER MINI - PURE GPIO 0 & 1 ULTRASONIC TEST
 * NO SERIAL (Zero UART interference on GPIO 20/21)
 * Real-time test of both directions on pins 0 & 1
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long distA = -1; // Trig=0, Echo=1
long distB = -1; // Trig=1, Echo=0
long durA = 0;
long durB = 0;

void addLog(String s) {
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long pingPins(int trig, int echo, long &outDur) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long dur = pulseIn(echo, HIGH, 30000);
  outDur = dur;
  if (dur <= 0) return -1;
  return (long)(dur * 0.034 / 2);
}

void setup() {
  // KHONG DUNG SERIAL DE TRANH XUNG DOT PHAN CUNG
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

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());

  server.on("/distance", HTTP_GET, []() {
    long best = (distA >= 0) ? distA : distB;
    server.send(200, "text/plain", String(best));
  });

  server.on("/raw", HTTP_GET, []() {
    int p0 = digitalRead(0);
    int p1 = digitalRead(1);
    String s = "State: P0=" + String(p0) + ", P1=" + String(p1) + "\n";
    s += "Kieu A (Trig=0, Echo=1): Dist=" + String(distA) + " cm, Dur=" + String(durA) + " us\n";
    s += "Kieu B (Trig=1, Echo=0): Dist=" + String(distB) + " cm, Dur=" + String(durB) + " us";
    server.send(200, "text/plain", s);
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String s = "State: P0=" + String(digitalRead(0)) + ", P1=" + String(digitalRead(1)) + " | Temp=" + String(temp, 1) + " C\n";
    s += "A(0->1): " + String(distA) + " cm (" + String(durA) + " us) | B(1->0): " + String(distB) + " cm (" + String(durB) + " us)\n";
    s += logs;
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

  static unsigned long lastP = 0;
  unsigned long now = millis();
  if (now - lastP >= 70) {
    lastP = now;

    // Do Kieu A (Trig 0 -> Echo 1)
    distA = pingPins(0, 1, durA);
    delay(20);
    // Do Kieu B (Trig 1 -> Echo 0)
    distB = pingPins(1, 0, durB);

    long valid = (distA >= 0) ? distA : distB;
    if (valid >= 0) {
      static long lastValid = -999;
      if (abs(valid - lastValid) >= 2) {
        lastValid = valid;
        addLog("📏 DO DUOC: " + String(valid) + " cm (A=" + String(distA) + ", B=" + String(distB) + ")");
      }
    }
  }

  delay(5);
}
