/*
 * ESP32-C3 SUPER MINI - KẾT NỐI WI-FI "Trong Khang" & SÁNG ĐÈN LED
 * Tối ưu mát mẻ (80MHz), hỗ trợ nạp code không dây (Web OTA)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#define LED_PIN 8  // Đèn LED xanh trên bo ESP32-C3 Super Mini (mức LOW là SÁNG)

WebServer server(80);

void setup() {
  Serial.begin(115200);

  // Hạ xung nhịp 80MHz để chip luôn mát mẻ
  setCpuFrequencyMhz(80);

  // Cấu hình chân đèn LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Mới bật: Tắt đèn LED (chờ kết nối)

  // Kết nối vào Wi-Fi nhà "Trong Khang"
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true); // Bật tiết kiệm điện
  WiFi.begin("Trong Khang", "khongbiet123");

  Serial.println("Đang kết nối Wi-Fi...");

  // Chờ cho đến khi kết nối thành công
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Chớp nháy nhẹ báo hiệu đang dò tìm
  }

  // 🎉 KHI ĐÃ KẾT NỐI THÀNH CÔNG:
  // Ngừng dò tìm và BẬT SÁNG ĐỨNG YÊN ĐÈN LED XANH (mức LOW là SÁNG)
  digitalWrite(LED_PIN, LOW);
  Serial.println("\nĐÃ KẾT NỐI THÀNH CÔNG!");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());

  // Cổng Web OTA nạp code không dây qua Wi-Fi
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

  // Trang web xem nhiệt độ và kiểm tra
  server.on("/", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain; charset=utf-8", "ESP32-C3 OK! Nhiệt độ: " + String(temp, 1) + " độ C");
  });

  server.on("/temp", HTTP_GET, []() {
    float temp = temperatureRead();
    server.send(200, "text/plain", "Nhiet do: " + String(temp, 1) + " C");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  delay(10); // Nghỉ 10ms để CPU mát mẻ
}
