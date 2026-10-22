/*
 * ESP32-C3 SUPER MINI - SIMPLE WIFI AP & BLINK TEST
 * No external libs, pure AP mode + LED blink
 */
#include <WiFi.h>

#define LED_PIN 8

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Turn ON LED immediately on boot

  // Emit Wi-Fi AP immediately
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-TEST-WIFI", "12345678");

  Serial.println("\n==================================");
  Serial.println("🎉 DA PHAT SONG WI-FI THANH CONG!");
  Serial.println("SSID: ESP32-TEST-WIFI | Pass: 12345678");
  Serial.println("==================================");
}

void loop() {
  digitalWrite(LED_PIN, LOW);   // LED ON (Active Low)
  delay(500);
  digitalWrite(LED_PIN, HIGH);  // LED OFF
  delay(500);
}
