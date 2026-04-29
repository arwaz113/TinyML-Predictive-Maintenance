#include <WiFi.h>
#include <FirebaseESP32.h>
#include <esp_now.h>
#include <WebServer.h>
// ==================== USER CONFIGURATION ====================
// WiFi credentials (remove leading/trailing spaces)
#define WIFI_SSID " Galaxy F13"
#define WIFI_PASSWORD "password"
// Firebase
#define FIREBASE_HOST "motorguard-9320a-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "28DG8b9YWFWHw00m4E6uadchulfZtoVKV84x4uX1"
// ESP‑NOW peer MAC address (receiver)
uint8_t broadcastAddress[] = {0x70, 0x4B, 0xCA, 0x8F, 0x8F, 0xCC};
// Worst event threshold (how many "Worst" before sending OFF)
#define WORST_THRESHOLD 15
 
// UART2 pins for STM32 communication
#define STM32_RX_PIN 16
#define STM32_TX_PIN 17
 
// ==================== GLOBALS ====================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WebServer server(80);
 
String latestData = "waiting";
int worstCounter = 0;
bool actionTaken = false;   // true after threshold reached and OFF sent
 
// ESP‑NOW message structure
typedef struct struct_message { char command[32]; } struct_message;
struct_message myData;
 
// ==================== FUNCTION PROTOTYPES ====================
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status);
void sendEspNow(const char* msg);
void onWorstThresholdReached();
void handleRoot();
void setupWiFi();
void setupFirebase();
void setupEspNow();
 
// ==================== ESP‑NOW CALLBACK ====================
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ESP-NOW: Sent Success" : "ESP-NOW: Sent Fail");
}
 
void sendEspNow(const char* msg) {
  strcpy(myData.command, msg);
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
}
 
// ==================== ACTION ON THRESHOLD ====================
void onWorstThresholdReached() {
  Serial.println("*** WORST THRESHOLD REACHED! Sending ESP-NOW OFF ***");
  sendEspNow("OFF");
 
  // Optional: Update Firebase
  if (WiFi.status() == WL_CONNECTED) {
    Firebase.setString(fbdo, "/alert", "worst_threshold_reached");
    Firebase.setInt(fbdo, "/worstCount", worstCounter);
  }
}
 
// ==================== WEB PAGE ====================
void handleRoot() {
  // Prevent caching so refresh always fetches latest data
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
 
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";   // auto-refresh every 3 seconds
  html += "<title>Motor Guard Pro</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #0d1117; color: #c9d1d9; text-align: center; margin: 0; padding: 20px; }";
  html += ".container { max-width: 400px; margin: auto; background: #161b22; padding: 30px; border-radius: 20px; border: 1px solid #30363d; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }";
  html += "h1 { color: #58a6ff; font-size: 24px; margin-bottom: 25px; border-bottom: 1px solid #30363d; padding-bottom: 10px; }";
  html += ".status-box { background: #21262d; padding: 15px; border-radius: 12px; margin-bottom: 20px; }";
  html += ".label { font-size: 14px; color: #8b949e; text-transform: uppercase; letter-spacing: 1px; }";
  String statusColor = (latestData == "Worst") ? "#f85149" : "#3fb950";
  html += ".value { font-size: 28px; font-weight: bold; color: " + statusColor + "; margin-top: 5px; }";
  html += ".stop-btn { width: 100%; padding: 20px; background: #da3633; color: white; border: none; border-radius: 12px; font-size: 18px; font-weight: bold; cursor: pointer; transition: 0.3s; box-shadow: 0 4px 15px rgba(218, 54, 51, 0.4); }";
  html += ".stop-btn:hover { background: #f85149; transform: translateY(-2px); }";
  html += ".stop-btn:active { transform: translateY(0); }";
  html += "#msg { margin-top: 15px; color: #d29922; font-size: 14px; font-style: italic; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>MOTOR GUARD IIoT</h1>";
  html += "<div class='status-box'><div class='label'>Vibration Status</div><div class='value'>" + latestData + "</div></div>";
  html += "<div class='status-box'><div class='label'>Worst Anomaly Count</div><div class='value' style='color:#d29922;'>" + String(worstCounter) + " / " + String(WORST_THRESHOLD) + "</div></div>";
  html += "<button class='stop-btn' onclick='stopMotor()'>EMERGENCY STOP</button>";
  html += "<div id='msg'></div>";
  html += "</div>";
  html += "<script>function stopMotor(){";
  html += "document.getElementById('msg').innerText = 'Sending Kill Signal...';";
  html += "fetch('/stop_fan').then(response => {";
  html += "document.getElementById('msg').innerText = 'System Terminated Successfully!';";
  html += "document.getElementById('msg').style.color = '#3fb950';";
  html += "});}</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}
 
// ==================== WIFI SETUP (STA + AP FALLBACK) ====================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
 
  Serial.print("\nConnecting to WiFi SSID: '");
  Serial.print(WIFI_SSID);
  Serial.println("'");
 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    Serial.print("Channel: "); Serial.println(WiFi.channel());
  } else {
    Serial.println("\n[WiFi] Failed to connect.");
    Serial.println("Switching to Access Point mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MotorGuard_ESP", "12345678");
    Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());
  }
}
 
// ==================== FIREBASE SETUP ====================
void setupFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Serial.println("Firebase initialized");
  } else {
    Serial.println("Firebase skipped (no WiFi)");
  }
}
 
// ==================== ESP‑NOW SETUP ====================
void setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
 
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  // Use same channel as WiFi if connected, otherwise default channel 1
  if (WiFi.status() == WL_CONNECTED) {
    peerInfo.channel = WiFi.channel();
  } else {
    peerInfo.channel = 1;
  }
  peerInfo.encrypt = false;
 
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  } else {
    Serial.println("ESP-NOW peer added");
  }
}
 
// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("\nESP32 starting...");
 
  setupWiFi();       // tries STA, fallback to AP
  setupFirebase();   // only if STA connected
  setupEspNow();     // uses same channel as WiFi (or AP channel)
 
  // Web server
  server.on("/", handleRoot);
  server.on("/stop_fan", []() {
    sendEspNow("OFF");
    server.send(200, "text/plain", "OK");
  });
  server.begin();
  Serial.println("Web server started");
}
 
// ==================== LOOP ====================
void loop() {
  server.handleClient();   // non‑blocking web server
 
  // Read data from STM32 (UART2)
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    received.trim();   // remove \r or spaces
   
    if (received.length() > 0) {
      latestData = received;
      Serial.print("STM32: ");
      Serial.println(latestData);
     
      // Update worst counter logic
      if (received == "Bad") {
        worstCounter++;
        Serial.print("Bad counter: ");
        Serial.println(worstCounter);
       
        // Check threshold
        if (worstCounter >= WORST_THRESHOLD && !actionTaken) {
          actionTaken = true;
          onWorstThresholdReached();
        }
      }
      else if (received == "Good" || received == "Worst") {
        // Reset counter and action flag when condition improves
        worstCounter = 0;
        actionTaken = false;
        Serial.println("Counter reset (Good/Bad received)");
      }
     
      // Optional: Send latest status to Firebase (if WiFi connected)
      if (WiFi.status() == WL_CONNECTED) {
        Firebase.setString(fbdo, "/currentStatus", latestData);
        Firebase.setInt(fbdo, "/worstCount", worstCounter);
      }
    }
  }
 
  delay(10);   // small delay to avoid watchdog issues
}
