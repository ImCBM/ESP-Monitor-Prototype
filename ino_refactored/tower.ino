/*
 * Tower Device - Fixed Relay/Measurement Node
 * 
 * Communication Flow:
 * 2. Receives DISCOVERY_PING from peers
 * 3. Calculates distance, responds with TOWER_RESPONSE
 * 5. Receives PEER_AGGREGATE_REPORT, validates passkey
 * 6. Forwards to Gateway as GATEWAY_RELAY
 * 
 * Hardware: Any ESP32 board
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// ============================================================================
//                         DEVICE CONFIGURATION
// ============================================================================
const char* TOWER_ID = "TOWER_001";
const char* TOWER_NAME = "Tower_1";  // Human-readable name
const char* SHARED_KEY = "ndrrmc_441";
const char* GATEWAY_PASSKEY = "tower_2_gateway";  // Required for relay to gateway
const int ESP_NOW_CHANNEL = 1;

// ============================================================================
//                         RSSI TO DISTANCE CONFIGURATION
// ============================================================================
const int RSSI_AT_1M = -40;           // RSSI at 1 meter (calibrate per environment)
const float PATH_LOSS_EXPONENT = 2.0; // Free space = 2.0, indoor = 2.5-3.5
const float MAX_DISTANCE = 100.0;     // Cap maximum distance

// ============================================================================
//                         STATISTICS
// ============================================================================
int discoveryPingsReceived = 0;
int aggregateReportsReceived = 0;
int relaysSent = 0;

bool espNowInitialized = false;

// ============================================================================
//                         RSSI TO DISTANCE CALCULATION
// ============================================================================
float calculateDistance(int rssi) {
  if (rssi >= 0) return 0.0;
  
  // Path loss formula: d = 10^((RSSI_ref - RSSI) / (10 * n))
  float distance = pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
  
  if (distance > MAX_DISTANCE) {
    distance = MAX_DISTANCE;
  }
  
  return distance;
}

// ============================================================================
//                         ESP-NOW CALLBACKS
// ============================================================================
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Send: %s\n", status == ESP_NOW_SEND_SUCCESS ? "✓" : "❌");
}

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -70;
  
  // Get sender MAC
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  
  // Convert data to string
  String receivedData = "";
  for (int i = 0; i < len; i++) {
    receivedData += (char)data[i];
  }
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, receivedData);
  
  if (error) {
    Serial.printf("❌ Invalid JSON from %s\n", macStr);
    return;
  }
  
  // Validate shared key
  String key = doc["shared_key"] | "";
  if (key != SHARED_KEY) {
    Serial.printf("❌ Invalid key from %s\n", macStr);
    return;
  }
  
  String msgType = doc["type"] | "";
  
  if (msgType == "DISCOVERY_PING") {
    handleDiscoveryPing(doc, info->src_addr, rssi);
  } else if (msgType == "PEER_AGGREGATE_REPORT") {
    handleAggregateReport(doc, info->src_addr);
  }
}

// ============================================================================
//                    2-3. HANDLE DISCOVERY PING & RESPOND
// ============================================================================
void handleDiscoveryPing(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
  discoveryPingsReceived++;
  
  String messageId = doc["message_id"] | "";
  String peerId = doc["peer_id"] | "unknown";
  
  // Calculate distance from RSSI
  float distance = calculateDistance(rssi);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("📥 DISCOVERY_PING Received");
  Serial.printf("   From: %s\n", peerId.c_str());
  Serial.printf("   Message ID: %s\n", messageId.c_str());
  Serial.printf("   RSSI: %d dBm → Distance: %.2fm\n", rssi, distance);
  Serial.println("════════════════════════════════════════");
  
  // Send TOWER_RESPONSE
  sendTowerResponse(senderMac, messageId, distance);
}

// ============================================================================
//                    3. SEND TOWER RESPONSE TO PEER
// ============================================================================
void sendTowerResponse(const uint8_t* peerMac, const String& messageId, float distance) {
  // Add peer if not already added
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMac, 6);
  peerInfo.channel = ESP_NOW_CHANNEL;
  peerInfo.encrypt = false;
  
  if (!esp_now_is_peer_exist(peerMac)) {
    esp_now_add_peer(&peerInfo);
  }
  
  JsonDocument doc;
  doc["type"] = "TOWER_RESPONSE";
  doc["message_id"] = messageId;  // Echo back same message_id
  doc["tower_id"] = TOWER_ID;
  doc["tower_name"] = TOWER_NAME;
  doc["distance"] = distance;
  doc["shared_key"] = SHARED_KEY;
  doc["timestamp"] = millis() / 1000;
  
  String message;
  serializeJson(doc, message);
  
  Serial.println("📤 Sending TOWER_RESPONSE");
  Serial.printf("   Tower: %s\n", TOWER_NAME);
  Serial.printf("   Distance: %.2fm\n", distance);
  
  esp_now_send(peerMac, (uint8_t*)message.c_str(), message.length());
}

// ============================================================================
//                    5-6. HANDLE AGGREGATE REPORT & RELAY
// ============================================================================
void handleAggregateReport(JsonDocument& doc, const uint8_t* senderMac) {
  aggregateReportsReceived++;
  
  String messageId = doc["message_id"] | "";
  String owner = doc["owner"] | "unknown";
  
  // Validate gateway passkey
  String passkey = doc["auth"]["passkey"] | "";
  if (passkey != GATEWAY_PASSKEY) {
    Serial.println("❌ Invalid gateway passkey - rejecting aggregate report");
    return;
  }
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("📥 PEER_AGGREGATE_REPORT Received");
  Serial.printf("   From: %s\n", owner.c_str());
  Serial.printf("   Message ID: %s\n", messageId.c_str());
  Serial.println("   ✓ Passkey validated");
  
  // Print tower data
  JsonArray towers = doc["towers"];
  Serial.printf("   Towers (%d):\n", towers.size());
  for (JsonObject tower : towers) {
    String name = tower["name"] | "?";
    float dist = tower["distance"] | 0.0;
    Serial.printf("   - %s: %.2fm\n", name.c_str(), dist);
  }
  Serial.println("════════════════════════════════════════");
  
  // Relay to gateway
  relayToGateway(doc);
}

// ============================================================================
//                    6. RELAY TO GATEWAY
// ============================================================================
void relayToGateway(JsonDocument& originalReport) {
  JsonDocument relayDoc;
  relayDoc["type"] = "GATEWAY_RELAY";
  relayDoc["relay_tower"] = TOWER_NAME;
  relayDoc["shared_key"] = SHARED_KEY;
  relayDoc["timestamp"] = millis() / 1000;
  
  // Include original payload
  JsonObject payload = relayDoc["payload"].to<JsonObject>();
  payload["owner"] = originalReport["owner"];
  payload["message_id"] = originalReport["message_id"];
  
  // Copy towers array
  JsonArray towers = payload["towers"].to<JsonArray>();
  for (JsonObject srcTower : originalReport["towers"].as<JsonArray>()) {
    JsonObject dstTower = towers.add<JsonObject>();
    dstTower["name"] = srcTower["name"];
    dstTower["distance"] = srcTower["distance"];
  }
  
  String message;
  serializeJson(relayDoc, message);
  
  Serial.println("\n📤 GATEWAY_RELAY");
  Serial.printf("   Relaying for: %s\n", originalReport["owner"].as<String>().c_str());
  
  // Broadcast to gateway (gateway will receive)
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcastAddr, (uint8_t*)message.c_str(), message.length());
  
  relaysSent++;
}

// ============================================================================
//                         ESP-NOW INITIALIZATION
// ============================================================================
void initESPNow() {
  if (espNowInitialized) return;
  
  Serial.println("Initializing ESP-NOW...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  delay(100);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    ESP.restart();
    return;
  }
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  
  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
  peerInfo.channel = ESP_NOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  espNowInitialized = true;
  Serial.println("✓ ESP-NOW initialized");
  Serial.printf("✓ Channel %d\n", ESP_NOW_CHANNEL);
}

// ============================================================================
//                              SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("      TOWER - Relay/Measurement Node");
  Serial.println("════════════════════════════════════════");
  Serial.printf("Tower ID: %s\n", TOWER_ID);
  Serial.printf("Tower Name: %s\n", TOWER_NAME);
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  Serial.println("════════════════════════════════════════");
  Serial.println("Flow:");
  Serial.println("  2. Receive DISCOVERY_PING");
  Serial.println("  3. Send TOWER_RESPONSE with distance");
  Serial.println("  5. Receive PEER_AGGREGATE_REPORT");
  Serial.println("  6. Relay to GATEWAY");
  Serial.println("════════════════════════════════════════\n");
  
  initESPNow();
  
  Serial.println("🚀 Tower Ready - Listening for peers...\n");
}

// ============================================================================
//                              MAIN LOOP
// ============================================================================
void loop() {
  // Tower is receive-only, all work happens in callbacks
  delay(100);
}
