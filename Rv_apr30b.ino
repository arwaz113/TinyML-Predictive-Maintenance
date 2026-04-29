#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
 
#define TRIAC_PIN  5
#define LED_PIN    2
 
typedef struct struct_message { char command[32]; } struct_message;
struct_message incomingData;
 
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  String cmd = String(incomingData.command);
  if (cmd == "OFF") {
    Serial.println("\n[RECEIVED] OFF command! Stopping fan.");
    digitalWrite(TRIAC_PIN, LOW);   // Fan OFF
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH); delay(100);
      digitalWrite(LED_PIN, LOW);  delay(100);
    }
    digitalWrite(LED_PIN, HIGH);
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(TRIAC_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(TRIAC_PIN, HIGH);   // Fan ON
  digitalWrite(LED_PIN, LOW);
 
  // WiFi station mode, no connection
  WiFi.mode(WIFI_STA);
  // Force channel to 1 (same as sender's AP)
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  Serial.println("WiFi channel set to 1 (matching sender)");
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Receiver ready. Waiting for 'OFF'...");
}
 
void loop() {
  delay(1000);

	}
