/*
 * ECO-CLEAN BIN - SAFE SERVO & COOLING
 * Fix: Pulse 1000-2000us, Auto-Detach (ledcWrite 0) after movement
 * No auto-rotation on boot (starts with 0mA idle)
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

// Ham quay an toan: Chuyen dong xong -> NGAT XUNG NGAY de mo to khong bi ket/nong
void rotateServo(int angle) {
  // Gioi han an toan cho SG90: 1000us (0 do) den 2000us (180 do)
  long pulse_us = map(angle, 0, 180, 1000, 2000);
  long duty = (pulse_us * 16384) / 20000;
  
  ledcWrite(0, duty);
  addLog("🤖 SERVO DANG QUAY DEN: " + String(angle) + " DO");
  
  // Cho 450ms de servo hoan tat quay
  delay(450);
  
  // 🔥 TRIET DE: DẬP TẮT XUNG PWM NGAY LẬP TỨC!
  // Giup mo to servo ngat dien 100% (0mA), banh rang tha long, khong bao gio bi ket hay nong!
  ledcWrite(0, 0);
  addLog("💤 DA NGAT DIEN MO TO (0mA) - THA LONG AN TOAN!");
}

void setup() {
  Serial.begin(115200);

  // Ha xung 80MHz de tiet kiem dien
  setCpuFrequencyMhz(80);
  pinMode(LED_PIN, OUTPUT);

  // Khoi tao PWM va DAT VE 0 NGAY - KHONG PHAT XUNG KHI VUA BAT NGUON
  ledcSetup(0, 50, 14);
  ledcAttachPin(SERVO_PIN, 0);
  ledcWrite(0, 0); // Xung = 0: Servo nam im hoan toan khi vua cap nguon!

  // Ket noi WiFi nha
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin("Trong Khang", "khongbiet123");
  addLog("❄️ KHOI DONG XONG! Chế độ an toàn: KHÔNG tự quay servo lúc bật nguồn.");

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
      rotateServo(a);
      server.send(200, "text/plain", "Da quay servo den: " + String(a) + " do va tha long motor");
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
    addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  }

  static unsigned long t = 0;
  if (millis() - t > 1000) {
    t = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  delay(10);
}
