/*
 * ESP32-C3 SUPER MINI - BARE BOARD TEMPERATURE & HEALTH CHECK
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

WebServer server(80);

void setup() {
  setCpuFrequencyMhz(80);
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW); // Sang LED

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(true);
  WiFi.begin("Trong Khang", "khongbiet123");
  while (WiFi.status() != WL_CONNECTED) delay(100);

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do chip: " + String(temp, 1) + " C");
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain; charset=utf-8", "ESP32-C3 Trần: Nhiệt độ = " + String(temp, 1) + " độ C (Mát mẻ)");
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
