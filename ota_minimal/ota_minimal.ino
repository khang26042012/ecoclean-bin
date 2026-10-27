/*
 * ECO-CLEAN BIN - ALL-PIN AUTONOMOUS SCANNER
 * Searches all possible pin combinations until the placed object is detected!
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8

WebServer server(80);
String logs = "";
long foundDist = -1;
String lockedPair = "Đang quét tất cả các chân...";
bool isLocked = false;
int lockedTrig = -1;
int lockedEcho = -1;

void addLog(String s) {
  Serial.println(s);
  logs += s + "\n";
  if (logs.length() > 3000) logs = logs.substring(logs.indexOf('\n') + 1);
}

long checkPulse(int trig, int echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(12);
  digitalWrite(trig, LOW);

  // pulseIn voi timeout 35000us (~6m)
  long dur = pulseIn(echo, HIGH, 35000);
  if (dur > 120 && dur < 24000) { // Hop le: 2cm den 400cm
    return (long)(dur * 0.034 / 2);
  }
  return -1;
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
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW); // Sang LED on dinh

  addLog("🎉 DA KET NOI WI-FI! IP: " + WiFi.localIP().toString());
  addLog("🔍 KHOI DONG CHE DO QUET LIEN TUC: Tu dong tim cap chan phat hien vat!");

  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(foundDist));
  });

  server.on("/log", HTTP_GET, []() {
    float temp = temperatureRead();
    String out = logs + "\n[LIVE] Khoang cach: " + String(foundDist) + " cm | " + lockedPair + " | Temp: " + String(temp, 1) + " C";
    server.send(200, "text/plain; charset=utf-8", out);
  });

  server.on("/status", HTTP_GET, []() {
    String s = "Dist=" + String(foundDist) + " cm | Pair=" + lockedPair;
    server.send(200, "text/plain", s);
  });

  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    String distText = (foundDist >= 0) ? String(foundDist) + " cm" : "Đang quét tìm vật cản...";
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>EcoClean Distance Finder</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#181818;color:#fff;}";
    html += ".card{background:#242424;border-radius:14px;padding:25px;max-width:380px;margin:15px auto;}";
    html += ".dist{font-size:48px;font-weight:bold;color:#00e676;margin:15px 0;}";
    html += ".cfg{color:#ffd54f;font-size:16px;}";
    html += "</style><meta http-equiv='refresh' content='1'></head><body>";
    html += "<div class='card'><h2>KẾT QUẢ ĐO VẬT CẢN</h2>";
    html += "<div class='dist'>" + distText + "</div>";
    html += "<p class='cfg'>" + lockedPair + "</p>";
    html += "<p style='color:#81c784;'>Nhiệt độ chip: " + String(temp, 1) + " &deg;C</p>";
    html += "<p><a href='/log' style='color:#ffb74d;'>Xem Nhật ký (Log)</a></p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
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

// Danh sach tat ca cac cap chan co the co:
// 1) Cap chan 0 & 1 (uu tien hang dau)
// 2) Cap chan 20 & 21
// 3) Cap chan 3 & 4
// 4) Cap chan 1 & 2
// 5) Cap chan 2 & 3
const int pairList[][2] = {
  {0, 1}, {1, 0},
  {20, 21}, {21, 20},
  {3, 4}, {4, 3},
  {1, 2}, {2, 1},
  {2, 3}, {3, 2},
  {0, 2}, {2, 0}
};
const int NUM_PAIRS = sizeof(pairList) / sizeof(pairList[0]);

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    WiFi.reconnect();
    delay(200);
    return;
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  static unsigned long lastRun = 0;
  unsigned long now = millis();
  if (now - lastRun >= 60) {
    lastRun = now;

    if (isLocked) {
      // Neu da khoa cap chan thanh cong: Do lien tuc cap chan nay
      long d = checkPulse(lockedTrig, lockedEcho);
      if (d >= 2 && d <= 350) {
        foundDist = d;
      }
    } else {
      // Quet lan luot tung cap chan cho den khi bat duoc song phan hoi!
      for (int i = 0; i < NUM_PAIRS; i++) {
        int t = pairList[i][0];
        int e = pairList[i][1];
        long d = checkPulse(t, e);
        if (d >= 2 && d <= 350) {
          foundDist = d;
          lockedTrig = t;
          lockedEcho = e;
          isLocked = true;
          lockedPair = "🎯 ĐÃ KHÓA THÀNH CÔNG: Trig=GPIO " + String(t) + ", Echo=GPIO " + String(e);
          addLog("🎉 THÀNH CÔNG RỒI! Bắt được khoảng cách: " + String(d) + " cm");
          addLog("🎯 Khóa cấu hình: Trig=GPIO " + String(t) + ", Echo=GPIO " + String(e));
          break;
        }
        delay(10);
      }
    }
  }

  delay(5);
}
