/*
 * ECO-CLEAN BIN - COOLING MODE
 * Underclock 80MHz, STA only (Trong Khang), Light Sleep, Temp sensor
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define SERVO_PIN 3
#define LED_PIN   8

WebServer server(80);
String logs = "";

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2000) logs = logs.substring(logs.indexOf('\n') + 1);
}

void setServo(int angle) {
  long duty = (map(angle, 0, 180, 500, 2400) * 16384) / 20000;
  ledcWrite(0, duty);
  addLog("🤖 SERVO DA QUAY DEN: " + String(angle) + " DO");
}

void setup() {
  Serial.begin(115200);

  // Underclock to 80MHz
  setCpuFrequencyMhz(80);

  pinMode(LED_PIN, OUTPUT);

  ledcSetup(0, 50, 14);
  ledcAttachPin(SERVO_PIN, 0);
  setServo(0);

  // Station only (Trong Khang) + Modem Sleep
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin("Trong Khang", "khongbiet123");
  addLog("❄️ DA HA XUNG 80MHz & TAT AP! Dang ket noi Wi-Fi Trong Khang...");

  // Endpoints
  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n🌡️ NHIET DO CHIP HIEN TAI: " + String(temp, 1) + " do C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do chip: " + String(temp, 1) + " C");
  });

  server.on("/servo", HTTP_GET, []() {
    if (server.hasArg("angle")) {
      int a = server.arg("angle").toInt();
      setServo(a);
      server.send(200, "text/plain", "Da quay servo den: " + String(a) + " do");
    } else {
      server.send(400, "text/plain", "Thieu goc quay");
    }
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

  static bool connected = false;
  if (!connected && WiFi.status() == WL_CONNECTED) {
    connected = true;
    addLog("🎉 DA KET NOI WI-FI THANH CONG! IP: " + WiFi.localIP().toString());
  }

  static unsigned long t = 0;
  if (millis() - t > 1000) {
    t = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  delay(10);
}
