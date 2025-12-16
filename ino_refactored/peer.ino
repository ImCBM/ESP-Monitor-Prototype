/*
 * Peer Device - Mobile/Node ESP32
 * 
 * Communication Flow:
 * 1. Broadcasts DISCOVERY_PING with UUID message_id
 * 2. Receives TOWER_RESPONSE from towers, verifies message_id
 * 3. Collects (tower_name, distance) pairs
 * 4. Sends PEER_AGGREGATE_REPORT to tower with passkey
 * 
 * Hardware: Any ESP32 board with button on GPIO 12
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// ============================================================================
//                         DEVICE CONFIGURATION
// ============================================================================
const char* PEER_ID = "PEER_001";
const char* PEER_OWNER = "user_alice";
const char* SHARED_KEY = "ndrrmc_441";
const char* GATEWAY_PASSKEY = "tower_2_gateway";  // Required for aggregate reports
const int ESP_NOW_CHANNEL = 1;

// ============================================================================
//                         TOWER RESPONSE TRACKING
// ============================================================================
struct TowerResponse {
  String towerName;
  float distance;
  unsigned long timestamp;
  bool valid;
};

const int MAX_TOWERS = 10;
TowerResponse collectedTowers[MAX_TOWERS];
int towerCount = 0;

String currentMessageId = "";
unsigned long pingTimestamp = 0;
const unsigned long RESPONSE_TIMEOUT = 5000;  // 5 seconds to collect responses
bool collectingResponses = false;

// ============================================================================
//                         STATE VARIABLES
// ============================================================================
unsigned long messageCounter = 0;
bool espNowInitialized = false;
const int BUTTON_PIN = 12;

// ============================================================================
//                         UUID GENERATION
// ============================================================================
String generateUUID() {
  // Generate a simple UUID-like string
  char uuid[37];
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  uint32_t r3 = esp_random();
  uint32_t r4 = esp_random();
  snprintf(uuid, sizeof(uuid), "%08lx-%04lx-%04lx-%04lx-%012llx",
           r1, (r2 >> 16) & 0xFFFF, r2 & 0xFFFF, (r3 >> 16) & 0xFFFF,
           ((uint64_t)(r3 & 0xFFFF) << 32) | r4);
  return String(uuid);
}

// ============================================================================
//                         ESP-NOW CALLBACKS
// ============================================================================
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "✓ SUCCESS" : "❌ FAILED");
}

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Convert data to string
  String receivedData = "";
  for (int i = 0; i < len; i++) {
    receivedData += (char)data[i];
  }
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, receivedData);
  
  if (error) {
    Serial.println("❌ Invalid JSON received");
    return;
  }
  
  String msgType = doc["type"] | "";
  
  if (msgType == "TOWER_RESPONSE") {
    processTowerResponse(doc);
  }
}

// ============================================================================
//                         TOWER RESPONSE PROCESSING
// ============================================================================
void processTowerResponse(JsonDocument& doc) {
  String receivedMessageId = doc["message_id"] | "";
  
  // Verify message_id matches our broadcast
  if (receivedMessageId != currentMessageId) {
    Serial.println("❌ Message ID mismatch - discarding response");
    return;
  }
  
  if (!collectingResponses) {
    Serial.println("❌ Not in collection mode - discarding response");
    return;
  }
  
  String towerName = doc["tower_name"] | "unknown";
  float distance = doc["distance"] | 0.0;
  
  // Store tower response
  if (towerCount < MAX_TOWERS) {
    collectedTowers[towerCount].towerName = towerName;
    collectedTowers[towerCount].distance = distance;
    collectedTowers[towerCount].timestamp = millis();
    collectedTowers[towerCount].valid = true;
    towerCount++;
    
    Serial.printf("✓ Tower response: %s at %.2fm\n", towerName.c_str(), distance);
  }
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
  
  // Set channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  delay(100);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }
  
  // Register callbacks
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
//                    1. BROADCAST DISCOVERY PING
// ============================================================================
void sendDiscoveryPing() {
  if (!espNowInitialized) initESPNow();
  
  // Clear previous responses
  towerCount = 0;
  for (int i = 0; i < MAX_TOWERS; i++) {
    collectedTowers[i].valid = false;
  }
  
  // Generate new message_id
  currentMessageId = generateUUID();
  pingTimestamp = millis();
  collectingResponses = true;
  
  // Build discovery ping
  JsonDocument doc;
  doc["type"] = "DISCOVERY_PING";
  doc["message_id"] = currentMessageId;
  doc["peer_id"] = PEER_ID;
  doc["owner"] = PEER_OWNER;
  doc["shared_key"] = SHARED_KEY;
  doc["timestamp"] = pingTimestamp / 1000;
  
  String message;
  serializeJson(doc, message);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("📡 DISCOVERY_PING Broadcast");
  Serial.printf("   Message ID: %s\n", currentMessageId.c_str());
  Serial.printf("   Peer ID: %s\n", PEER_ID);
  Serial.println("════════════════════════════════════════");
  
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcastAddr, (uint8_t*)message.c_str(), message.length());
  
  Serial.println("⏳ Waiting for tower responses...\n");
}

// ============================================================================
//                    5. SEND AGGREGATE REPORT TO TOWER
// ============================================================================
void sendAggregateReport() {
  if (towerCount == 0) {
    Serial.println("❌ No tower responses collected");
    return;
  }
  
  JsonDocument doc;
  doc["type"] = "PEER_AGGREGATE_REPORT";
  doc["message_id"] = currentMessageId;
  doc["owner"] = PEER_ID;
  doc["shared_key"] = SHARED_KEY;
  
  // Add collected towers
  JsonArray towers = doc["towers"].to<JsonArray>();
  for (int i = 0; i < towerCount; i++) {
    if (collectedTowers[i].valid) {
      JsonObject tower = towers.add<JsonObject>();
      tower["name"] = collectedTowers[i].towerName;
      tower["distance"] = collectedTowers[i].distance;
    }
  }
  
  // Add authentication with gateway passkey
  JsonObject auth = doc["auth"].to<JsonObject>();
  auth["passkey"] = GATEWAY_PASSKEY;
  
  doc["timestamp"] = millis() / 1000;
  
  String message;
  serializeJson(doc, message);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("📤 PEER_AGGREGATE_REPORT");
  Serial.printf("   Message ID: %s\n", currentMessageId.c_str());
  Serial.printf("   Towers: %d\n", towerCount);
  for (int i = 0; i < towerCount; i++) {
    Serial.printf("   - %s: %.2fm\n", 
                  collectedTowers[i].towerName.c_str(), 
                  collectedTowers[i].distance);
  }
  Serial.println("════════════════════════════════════════");
  
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcastAddr, (uint8_t*)message.c_str(), message.length());
}

// ============================================================================
//                              SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("       PEER DEVICE - Mobile Node");
  Serial.println("════════════════════════════════════════");
  Serial.printf("Peer ID: %s\n", PEER_ID);
  Serial.printf("Owner: %s\n", PEER_OWNER);
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  Serial.println("════════════════════════════════════════");
  Serial.println("Flow:");
  Serial.println("  1. Button press → DISCOVERY_PING");
  Serial.println("  2. Collect TOWER_RESPONSE (5s)");
  Serial.println("  3. Send PEER_AGGREGATE_REPORT");
  Serial.println("════════════════════════════════════════\n");
  
  initESPNow();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("🚀 Ready - Press button to start discovery\n");
}

// ============================================================================
//                              MAIN LOOP
// ============================================================================
void loop() {
  // Check for response timeout
  if (collectingResponses && (millis() - pingTimestamp > RESPONSE_TIMEOUT)) {
    collectingResponses = false;
    Serial.printf("\n⏱️ Collection timeout - Got %d tower responses\n", towerCount);
    
    if (towerCount > 0) {
      // Auto-send aggregate report
      sendAggregateReport();
    }
  }
  
  // Handle button press - start discovery
  if (digitalRead(BUTTON_PIN) == LOW && !collectingResponses) {
    Serial.println("\n🔘 Button pressed - Starting discovery");
    sendDiscoveryPing();
    delay(300);  // Debounce
  }
  
  delay(10);
}
