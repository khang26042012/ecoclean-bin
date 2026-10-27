/*
 * ESP32-C3 SUPER MINI - VOLTMETER ON PIN 0 AND PIN 1
 * Uses built-in 12-bit ADC to measure exact voltages on both lines!
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

WebServer server(80);

void setup() {
  setCpuFrequencyMhz(80);
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);

  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Trong Khang", "khongbiet123");
  while (WiFi.status() != WL_CONNECTED) delay(100);

  server.on("/volt", HTTP_GET, []() {
    int raw0 = analogRead(0);
    int raw1 = analogRead(1);
    float v0 = (raw0 / 4095.0) * 3.3;
    float v1 = (raw1 / 4095.0) * 3.3;
    String s = "PIN 0: Raw=" + String(raw0) + " (" + String(v0, 2) + "V)\n";
    s += "PIN 1: Raw=" + String(raw1) + " (" + String(v1, 2) + "V)";
    server.send(200, "text/plain", s);
  });

  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", "RESTART...");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
    else if (upload.status == UPLOAD_FILE_WRITE) Update.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END) Update.end(true);
  });

  server.begin();
}

void loop() {
  server.handleClient();
  delay(10);
}
