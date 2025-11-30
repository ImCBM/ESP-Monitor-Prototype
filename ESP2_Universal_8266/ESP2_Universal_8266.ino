/*
 * ESP2 Universal ESP8266 - ESP-NOW Only with Distance & Triangulation
 * 
 * This is the ESP8266 port focusing on:
 * - ESP-NOW peer-to-peer communication
 * - RSSI-based distance estimation
 * - Triangulation and relative positioning
 * - Monitor gateway communication
 * - NO WiFi functionality (ESP-NOW only)
 * 
 * Hardware: ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
 * 
 * Setup Instructions:
 * 1. Install ESP8266 Board Support via Board Manager
 * 2. Install ArduinoJson library
 * 3. Configure device settings below
 * 4. Upload to ESP8266
 */

// ============================================================================
//                           LIBRARY INCLUDES
// ============================================================================

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

// ============================================================================
//                         DEVICE CONFIGURATION
// ============================================================================
// CHANGE THESE FOR EACH ESP2 DEVICE
const char* DEVICE_ID = "ESP2_8266_001";          // Unique device identifier
const char* DEVICE_OWNER = "user_alice";          // Device owner name
const char* SHARED_KEY = "ESP2_NETWORK_KEY";      // Authentication key

// Protocol Configuration
const char* PROTOCOL_VERSION = "5.0";
const char* ESP2_VERSION = "2.0.0-ESP8266";
const char* DEVICE_TYPE = "ESP2_8266";
const char* FIRMWARE_VERSION = "2.0.0";
const int ESP_NOW_CHANNEL = 1;                    // Fixed channel

// ============================================================================
//                     POSITIONING CONFIGURATION
// ============================================================================
const unsigned long POSITIONING_INTERVAL = 10000;     // Update every 10 seconds
const int MIN_PEERS_FOR_POSITIONING = 1;
const int MIN_PEERS_FOR_TRIANGULATION = 3;
const float RSSI_CALIBRATION_DISTANCE = 1.0;         // Reference distance (1 meter)
const int RSSI_CALIBRATION_VALUE = -40;              // RSSI at reference distance
const float PATH_LOSS_EXPONENT = 2.0;
const float MAX_POSITIONING_DISTANCE = 100.0;        // Max reliable distance (meters)

// ============================================================================
//                        DATA STRUCTURES
// ============================================================================

// Direction enumeration
enum Direction {
  DIR_UNKNOWN = 0,
  DIR_NORTH = 1,
  DIR_NORTHEAST = 2,
  DIR_EAST = 3,
  DIR_SOUTHEAST = 4,
  DIR_SOUTH = 5,
  DIR_SOUTHWEST = 6,
  DIR_WEST = 7,
  DIR_NORTHWEST = 8
};

struct Position {
  float x;
  float y;
  bool isValid;
  unsigned long lastUpdated;
};

struct RelativePosition {
  float distance;
  Direction direction;
  float confidence;
  unsigned long lastUpdated;
  bool isValid;
};

struct PeerDevice {
  String deviceId;
  String owner;
  String macAddress;
  String deviceType;
  String firmwareVersion;
  int rssi;
  unsigned long lastSeen;
  unsigned long firstSeen;
  bool handshakeComplete;
  bool validated;
  
  // Positioning data
  RelativePosition relativePos;
  std::vector<int> rssiHistory;
  Position absolutePos;
  bool supportsTriangulation;
};

// ============================================================================
//                      TIMING & OPERATIONAL VARIABLES
// ============================================================================

unsigned long lastPeerDiscovery = 0;
const unsigned long PEER_DISCOVERY_INTERVAL = 5000;   // Ping every 5 seconds

unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 10000;       // Send data every 10 seconds

unsigned long lastPositioning = 0;

unsigned long messageCounter = 0;
String lastHandshakeMessageId = "";

// ============================================================================
//                         PEER MANAGEMENT
// ============================================================================

const int MAX_PEERS = 10;
PeerDevice knownPeers[MAX_PEERS];
int peerCount = 0;

Position myPosition = {0.0, 0.0, false, 0};
bool hasReferencePosition = false;

// ESP1 Gateway MAC (broadcast address)
uint8_t gatewayMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
//                        FUNCTION DECLARATIONS
// ============================================================================

void initESPNow();
void sendPeerDiscoveryPing();
void sendHandshakeResponse(const String& replyToMessageId, const uint8_t* peerMac);
void sendDataMessage();
void onESPNowReceive(uint8_t *mac, uint8_t *data, uint8_t len);
void onDataSent(uint8_t *mac, uint8_t status);
bool validateEnvelope(JsonDocument& doc);
String generateMessageId(const String& messageType);
void processIncomingMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi);
void addOrUpdatePeer(const String& deviceId, const String& owner, const String& macAddr, int rssi);
void printKnownPeers();
String macToString(const uint8_t* mac);
bool validatePeerCredentials(JsonDocument& doc);
void sendEnhancedHandshake(const String& replyToMessageId, const uint8_t* peerMac);
bool isPeerTrusted(const String& deviceId);

// Positioning functions
void performTriangulation();
void performDistanceMeasurement();
float calculateDistanceFromRSSI(int rssi);
Direction calculateRelativeDirection(const String& peerId1, const String& peerId2, const String& targetPeerId);
void updatePeerPosition(const String& deviceId, int rssi);
void sendTriangulationPing();
void processTriangulationData(JsonDocument& doc, int rssi);
String directionToString(Direction dir);
Direction stringToDirection(const String& dirStr);
void printPositioningSummary();
bool hasEnoughPeersForPositioning();
bool hasEnoughPeersForTriangulation();
void estimateRelativePositions();
void updateRSSIHistory(const String& deviceId, int rssi);

// ============================================================================
//                          SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP2 ESP8266 - ESP-NOW Only");
  Serial.println("=================================");
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.printf("Owner: %s\n", DEVICE_OWNER);
  Serial.printf("Device Type: %s\n", DEVICE_TYPE);
  Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
  Serial.println("=================================\n");
  
  // Set WiFi mode for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Initialize ESP-NOW
  initESPNow();
  
  Serial.println("Features Active:");
  Serial.println("  ✓ ESP-NOW Communication");
  Serial.println("  ✓ Peer Discovery");
  Serial.println("  ✓ Distance Measurement");
  Serial.println("  ✓ Triangulation");
  Serial.println("  ✓ Monitor Gateway Pinging\n");
}

// ============================================================================
//                          MAIN LOOP FUNCTION
// ============================================================================

void loop() {
  unsigned long currentMillis = millis();
  
  // Peer discovery ping
  if (currentMillis - lastPeerDiscovery >= PEER_DISCOVERY_INTERVAL) {
    lastPeerDiscovery = currentMillis;
    sendPeerDiscoveryPing();
  }
  
  // Send data to monitor
  if (currentMillis - lastDataSend >= DATA_SEND_INTERVAL) {
    lastDataSend = currentMillis;
    sendDataMessage();
  }
  
  // Positioning and triangulation
  if (currentMillis - lastPositioning >= POSITIONING_INTERVAL) {
    lastPositioning = currentMillis;
    
    if (hasEnoughPeersForTriangulation()) {
      performTriangulation();
    } else if (hasEnoughPeersForPositioning()) {
      performDistanceMeasurement();
    }
  }
  
  delay(10);
}

// ============================================================================
//                        ESP-NOW INITIALIZATION
// ============================================================================

void initESPNow() {
  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW init failed");
    ESP.restart();
    return;
  }
  
  Serial.println("✅ ESP-NOW initialized");
  
  // Set ESP-NOW role (ESP8266 uses different role system)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  
  // Register callbacks
  esp_now_register_recv_cb(onESPNowReceive);
  esp_now_register_send_cb(onDataSent);
  
  // Add broadcast peer for gateway
  esp_now_add_peer(gatewayMAC, ESP_NOW_ROLE_COMBO, ESP_NOW_CHANNEL, NULL, 0);
  
  Serial.println("✅ ESP-NOW ready for peer discovery");
}

// ============================================================================
//                       PEER DISCOVERY MESSAGING
// ============================================================================

void sendPeerDiscoveryPing() {
  StaticJsonDocument<512> doc;
  
  // Envelope structure
  doc["envelope_version"] = "1.0";
  doc["message_id"] = generateMessageId("PING");
  doc["message_type"] = "PING";
  doc["timestamp"] = millis();
  doc["sender_device_id"] = DEVICE_ID;
  doc["sender_owner"] = DEVICE_OWNER;
  
  // Payload
  JsonObject payload = doc.createNestedObject("payload");
  payload["device_type"] = DEVICE_TYPE;
  payload["firmware_version"] = FIRMWARE_VERSION;
  payload["protocol_version"] = PROTOCOL_VERSION;
  payload["shared_key"] = SHARED_KEY;
  payload["rssi"] = WiFi.RSSI();
  
  // Capabilities
  JsonArray capabilities = payload.createNestedArray("capabilities");
  capabilities.add("triangulation");
  capabilities.add("distance_measurement");
  capabilities.add("espnow");
  
  // Positioning status
  if (peerCount > 0) {
    payload["peer_count"] = peerCount;
    payload["has_positioning"] = hasEnoughPeersForPositioning();
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send to broadcast address (gateway)
  esp_now_send(gatewayMAC, (uint8_t*)jsonString.c_str(), jsonString.length());
  
  Serial.println("📡 Sent discovery ping to gateway");
}

void sendHandshakeResponse(const String& replyToMessageId, const uint8_t* peerMac) {
  sendEnhancedHandshake(replyToMessageId, peerMac);
}

void sendEnhancedHandshake(const String& replyToMessageId, const uint8_t* peerMac) {
  StaticJsonDocument<512> doc;
  
  doc["envelope_version"] = "1.0";
  doc["message_id"] = generateMessageId("HANDSHAKE");
  doc["message_type"] = "HANDSHAKE_RESPONSE";
  doc["timestamp"] = millis();
  doc["sender_device_id"] = DEVICE_ID;
  doc["sender_owner"] = DEVICE_OWNER;
  doc["reply_to"] = replyToMessageId;
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["device_type"] = DEVICE_TYPE;
  payload["firmware_version"] = FIRMWARE_VERSION;
  payload["protocol_version"] = PROTOCOL_VERSION;
  payload["shared_key"] = SHARED_KEY;
  payload["handshake_status"] = "ACCEPTED";
  payload["peer_validated"] = true;
  
  JsonArray capabilities = payload.createNestedArray("capabilities");
  capabilities.add("triangulation");
  capabilities.add("distance_measurement");
  capabilities.add("espnow");
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  esp_now_send((uint8_t*)peerMac, (uint8_t*)jsonString.c_str(), jsonString.length());
  
  Serial.printf("🤝 Sent handshake response to %s\n", macToString(peerMac).c_str());
}

void sendDataMessage() {
  StaticJsonDocument<1024> doc;
  
  doc["envelope_version"] = "1.0";
  doc["message_id"] = generateMessageId("DATA");
  doc["message_type"] = "DATA";
  doc["timestamp"] = millis();
  doc["sender_device_id"] = DEVICE_ID;
  doc["sender_owner"] = DEVICE_OWNER;
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["device_type"] = DEVICE_TYPE;
  payload["uptime"] = millis();
  payload["free_heap"] = ESP.getFreeHeap();
  payload["peer_count"] = peerCount;
  
  // Add positioning data if available
  if (peerCount > 0) {
    JsonArray peers = payload.createNestedArray("nearby_peers");
    
    for (int i = 0; i < peerCount; i++) {
      JsonObject peerObj = peers.createNestedObject();
      peerObj["device_id"] = knownPeers[i].deviceId;
      peerObj["owner"] = knownPeers[i].owner;
      peerObj["rssi"] = knownPeers[i].rssi;
      
      if (knownPeers[i].relativePos.isValid) {
        peerObj["distance"] = knownPeers[i].relativePos.distance;
        peerObj["direction"] = directionToString(knownPeers[i].relativePos.direction);
        peerObj["confidence"] = knownPeers[i].relativePos.confidence;
      }
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send to gateway
  esp_now_send(gatewayMAC, (uint8_t*)jsonString.c_str(), jsonString.length());
  
  Serial.println("📤 Sent data message to gateway");
}

// ============================================================================
//                        MESSAGE HANDLING
// ============================================================================

void onESPNowReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  // Get RSSI (ESP8266 doesn't provide RSSI in callback, estimate from WiFi)
  int rssi = WiFi.RSSI();
  if (rssi == 0 || rssi > 0) {
    rssi = -60; // Default value if unavailable
  }
  
  char buffer[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\0';
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  
  if (error) {
    Serial.println("❌ JSON parsing failed");
    return;
  }
  
  if (!validateEnvelope(doc)) {
    Serial.println("❌ Invalid envelope");
    return;
  }
  
  processIncomingMessage(doc, mac, rssi);
}

bool validateEnvelope(JsonDocument& doc) {
  if (!doc.containsKey("envelope_version") || 
      !doc.containsKey("message_id") ||
      !doc.containsKey("message_type") ||
      !doc.containsKey("timestamp") ||
      !doc.containsKey("sender_device_id") ||
      !doc.containsKey("sender_owner") ||
      !doc.containsKey("payload")) {
    return false;
  }
  return true;
}

String generateMessageId(const String& messageType) {
  return DEVICE_ID + String("_") + messageType + String("_") + String(messageCounter++);
}

void processIncomingMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
  String messageType = doc["message_type"].as<String>();
  String messageId = doc["message_id"].as<String>();
  String senderDevice = doc["sender_device_id"].as<String>();
  String senderOwner = doc["sender_owner"].as<String>();
  
  Serial.printf("📨 Received %s from %s (RSSI: %d)\n", messageType.c_str(), senderDevice.c_str(), rssi);
  
  // Validate credentials
  if (!validatePeerCredentials(doc)) {
    Serial.println("❌ Peer validation failed");
    return;
  }
  
  String macAddr = macToString(senderMac);
  addOrUpdatePeer(senderDevice, senderOwner, macAddr, rssi);
  
  if (messageType == "PING") {
    // Prevent handshake loops
    if (messageId == lastHandshakeMessageId) {
      return;
    }
    lastHandshakeMessageId = messageId;
    
    sendHandshakeResponse(messageId, senderMac);
    
  } else if (messageType == "HANDSHAKE_RESPONSE") {
    JsonObject payload = doc["payload"];
    String status = payload["handshake_status"].as<String>();
    
    if (status == "ACCEPTED") {
      // Mark peer as validated
      for (int i = 0; i < peerCount; i++) {
        if (knownPeers[i].deviceId == senderDevice) {
          knownPeers[i].handshakeComplete = true;
          knownPeers[i].validated = true;
          Serial.printf("✅ Handshake complete with %s\n", senderDevice.c_str());
          break;
        }
      }
    }
    
  } else if (messageType == "TRIANGULATION_PING") {
    processTriangulationData(doc, rssi);
    
  } else if (messageType == "DATA") {
    // Process data messages from other ESP2s
    updatePeerPosition(senderDevice, rssi);
  }
}

void addOrUpdatePeer(const String& deviceId, const String& owner, const String& macAddr, int rssi) {
  // Check if peer exists
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].deviceId == deviceId) {
      knownPeers[i].rssi = rssi;
      knownPeers[i].lastSeen = millis();
      updateRSSIHistory(deviceId, rssi);
      return;
    }
  }
  
  // Add new peer
  if (peerCount < MAX_PEERS) {
    knownPeers[peerCount].deviceId = deviceId;
    knownPeers[peerCount].owner = owner;
    knownPeers[peerCount].macAddress = macAddr;
    knownPeers[peerCount].rssi = rssi;
    knownPeers[peerCount].lastSeen = millis();
    knownPeers[peerCount].firstSeen = millis();
    knownPeers[peerCount].handshakeComplete = false;
    knownPeers[peerCount].validated = false;
    knownPeers[peerCount].supportsTriangulation = true;
    knownPeers[peerCount].relativePos.isValid = false;
    
    // Add ESP-NOW peer
    uint8_t mac[6];
    sscanf(macAddr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, ESP_NOW_CHANNEL, NULL, 0);
    
    peerCount++;
    Serial.printf("➕ Added peer: %s (%s)\n", deviceId.c_str(), owner.c_str());
  }
}

void printKnownPeers() {
  Serial.println("\n=== Known Peers ===");
  for (int i = 0; i < peerCount; i++) {
    Serial.printf("%d. %s (%s) - RSSI: %d, Validated: %s\n",
                  i + 1,
                  knownPeers[i].deviceId.c_str(),
                  knownPeers[i].owner.c_str(),
                  knownPeers[i].rssi,
                  knownPeers[i].validated ? "Yes" : "No");
    
    if (knownPeers[i].relativePos.isValid) {
      Serial.printf("   Distance: %.2fm, Direction: %s\n",
                    knownPeers[i].relativePos.distance,
                    directionToString(knownPeers[i].relativePos.direction).c_str());
    }
  }
  Serial.println("==================\n");
}

String macToString(const uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void onDataSent(uint8_t *mac, uint8_t status) {
  // Callback for send status (optional debugging)
}

// ============================================================================
//                      SECURITY VALIDATION
// ============================================================================

bool validatePeerCredentials(JsonDocument& doc) {
  JsonObject payload = doc["payload"];
  
  if (!payload.containsKey("shared_key")) {
    return false;
  }
  
  String receivedKey = payload["shared_key"].as<String>();
  if (receivedKey != SHARED_KEY) {
    Serial.println("❌ Invalid shared key");
    return false;
  }
  
  return true;
}

bool isPeerTrusted(const String& deviceId) {
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].deviceId == deviceId && knownPeers[i].validated) {
      return true;
    }
  }
  return false;
}

// ============================================================================
//                   TRIANGULATION & POSITIONING FUNCTIONS
// ============================================================================

float calculateDistanceFromRSSI(int rssi) {
  if (rssi >= 0) return 0.0;
  
  // Path loss formula: d = 10^((RSSI_cal - RSSI) / (10 * n))
  float distance = pow(10.0, (RSSI_CALIBRATION_VALUE - rssi) / (10.0 * PATH_LOSS_EXPONENT));
  
  // Clamp to reasonable range
  if (distance > MAX_POSITIONING_DISTANCE) {
    distance = MAX_POSITIONING_DISTANCE;
  }
  
  return distance;
}

void updateRSSIHistory(const String& deviceId, int rssi) {
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].deviceId == deviceId) {
      knownPeers[i].rssiHistory.push_back(rssi);
      if (knownPeers[i].rssiHistory.size() > 10) {
        knownPeers[i].rssiHistory.erase(knownPeers[i].rssiHistory.begin());
      }
      break;
    }
  }
}

void updatePeerPosition(const String& deviceId, int rssi) {
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].deviceId == deviceId) {
      updateRSSIHistory(deviceId, rssi);
      
      // Calculate average RSSI
      int avgRSSI = rssi;
      if (knownPeers[i].rssiHistory.size() > 0) {
        int sum = 0;
        for (int val : knownPeers[i].rssiHistory) {
          sum += val;
        }
        avgRSSI = sum / knownPeers[i].rssiHistory.size();
      }
      
      float distance = calculateDistanceFromRSSI(avgRSSI);
      
      knownPeers[i].relativePos.distance = distance;
      knownPeers[i].relativePos.confidence = min(1.0f, knownPeers[i].rssiHistory.size() / 10.0f);
      knownPeers[i].relativePos.lastUpdated = millis();
      knownPeers[i].relativePos.isValid = true;
      
      Serial.printf("📍 Updated position for %s: %.2fm (RSSI: %d)\n",
                    deviceId.c_str(), distance, avgRSSI);
      break;
    }
  }
}

void estimateRelativePositions() {
  if (peerCount < 2) return;
  
  // Simple relative direction estimation based on RSSI comparison
  for (int i = 0; i < peerCount; i++) {
    if (!knownPeers[i].relativePos.isValid) continue;
    
    // Compare with other peers to estimate direction
    int strongerNorth = 0, strongerSouth = 0;
    
    for (int j = 0; j < peerCount; j++) {
      if (i == j || !knownPeers[j].relativePos.isValid) continue;
      
      // Simplified directional logic (would need actual coordinates in real scenario)
      if (knownPeers[j].rssi > knownPeers[i].rssi) {
        strongerNorth++;
      } else {
        strongerSouth++;
      }
    }
    
    // Assign basic direction based on comparison
    if (strongerNorth > strongerSouth) {
      knownPeers[i].relativePos.direction = DIR_NORTH;
    } else {
      knownPeers[i].relativePos.direction = DIR_SOUTH;
    }
  }
}

void performDistanceMeasurement() {
  Serial.println("\n🎯 Performing distance measurement...");
  
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].validated && knownPeers[i].rssi != 0) {
      updatePeerPosition(knownPeers[i].deviceId, knownPeers[i].rssi);
    }
  }
  
  printPositioningSummary();
}

void performTriangulation() {
  Serial.println("\n📐 Performing triangulation...");
  
  // Update all peer positions
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].validated && knownPeers[i].rssi != 0) {
      updatePeerPosition(knownPeers[i].deviceId, knownPeers[i].rssi);
    }
  }
  
  // Estimate relative directions
  estimateRelativePositions();
  
  // Send triangulation ping
  sendTriangulationPing();
  
  printPositioningSummary();
}

void sendTriangulationPing() {
  StaticJsonDocument<1024> doc;
  
  doc["envelope_version"] = "1.0";
  doc["message_id"] = generateMessageId("TRIANGULATION_PING");
  doc["message_type"] = "TRIANGULATION_PING";
  doc["timestamp"] = millis();
  doc["sender_device_id"] = DEVICE_ID;
  doc["sender_owner"] = DEVICE_OWNER;
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["request_type"] = "position_update";
  payload["shared_key"] = SHARED_KEY;
  
  // Include current peer positions
  JsonArray peers = payload.createNestedArray("known_peers");
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].relativePos.isValid) {
      JsonObject peerObj = peers.createNestedObject();
      peerObj["device_id"] = knownPeers[i].deviceId;
      peerObj["distance"] = knownPeers[i].relativePos.distance;
      peerObj["rssi"] = knownPeers[i].rssi;
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send to all known peers
  for (int i = 0; i < peerCount; i++) {
    uint8_t mac[6];
    sscanf(knownPeers[i].macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    esp_now_send(mac, (uint8_t*)jsonString.c_str(), jsonString.length());
  }
  
  Serial.println("📡 Sent triangulation ping to peers");
}

void processTriangulationData(JsonDocument& doc, int rssi) {
  String senderDevice = doc["sender_device_id"].as<String>();
  updatePeerPosition(senderDevice, rssi);
}

String directionToString(Direction dir) {
  switch (dir) {
    case DIR_NORTH: return "North";
    case DIR_NORTHEAST: return "Northeast";
    case DIR_EAST: return "East";
    case DIR_SOUTHEAST: return "Southeast";
    case DIR_SOUTH: return "South";
    case DIR_SOUTHWEST: return "Southwest";
    case DIR_WEST: return "West";
    case DIR_NORTHWEST: return "Northwest";
    default: return "Unknown";
  }
}

Direction stringToDirection(const String& dirStr) {
  if (dirStr == "North") return DIR_NORTH;
  if (dirStr == "Northeast") return DIR_NORTHEAST;
  if (dirStr == "East") return DIR_EAST;
  if (dirStr == "Southeast") return DIR_SOUTHEAST;
  if (dirStr == "South") return DIR_SOUTH;
  if (dirStr == "Southwest") return DIR_SOUTHWEST;
  if (dirStr == "West") return DIR_WEST;
  if (dirStr == "Northwest") return DIR_NORTHWEST;
  return DIR_UNKNOWN;
}

bool hasEnoughPeersForPositioning() {
  int validPeers = 0;
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].validated) validPeers++;
  }
  return validPeers >= MIN_PEERS_FOR_POSITIONING;
}

bool hasEnoughPeersForTriangulation() {
  int validPeers = 0;
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].validated && knownPeers[i].supportsTriangulation) {
      validPeers++;
    }
  }
  return validPeers >= MIN_PEERS_FOR_TRIANGULATION;
}

void printPositioningSummary() {
  Serial.println("\n=== Positioning Summary ===");
  Serial.printf("Total Peers: %d\n", peerCount);
  
  for (int i = 0; i < peerCount; i++) {
    if (knownPeers[i].relativePos.isValid) {
      Serial.printf("  %s: %.2fm %s (Confidence: %.0f%%)\n",
                    knownPeers[i].deviceId.c_str(),
                    knownPeers[i].relativePos.distance,
                    directionToString(knownPeers[i].relativePos.direction).c_str(),
                    knownPeers[i].relativePos.confidence * 100);
    }
  }
  Serial.println("==========================\n");
}
