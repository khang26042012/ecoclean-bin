/*
 * ESP32-C3 SUPER MINI - SOLID LED ALWAYS ON WHEN WIFI CONNECTED
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long currentDist = -1;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 2500) logs = logs.substring(logs.indexOf('\n') + 1);
}

long pingSensor(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(15);
  digitalWrite(trig, LOW);

  long dur = pulseIn(echo, HIGH, 25000);
  if (dur <= 0) return -1;
  return (long)(dur * 0.034 / 2);
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
    delay(150);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Chop nhay bao dang ket noi
  }
  
  // 🔥 ĐÈN XANH LUÔN SÁNG ĐỨNG YÊN KHI CÓ WI-FI!
  digitalWrite(LED_PIN, LOW); 
  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(currentDist));
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[Hien tai] Dist: " + String(currentDist) + " cm | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
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

  // Luon giu den LED sang khi WiFi da ket noi
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  }

  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck >= 80) {
    lastCheck = now;

    long dA = pingSensor(0, 1);
    if (dA >= 2 && dA <= 350) {
      currentDist = dA;
    } else {
      long dB = pingSensor(1, 0);
      if (dB >= 2 && dB <= 350) {
        currentDist = dB;
      } else {
        currentDist = -1;
      }
    }

    if (currentDist >= 0 && currentDist <= 30) {
      static long lastLogged = -1;
      if (abs(currentDist - lastLogged) >= 2) {
        lastLogged = currentDist;
        addLog("✋ PHÁT HIỆN TAY! Khoảng cách: " + String(currentDist) + " cm");
      }
    }
  }

  delay(5);
}
