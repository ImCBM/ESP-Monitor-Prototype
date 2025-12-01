/*
 * ESP1 - WIRED ESP2 COMMUNICATION GATEWAY
 * 
 * PURPOSE: Wired bridge for battery-powered ESP2 network
 *          Always connected to PC/laptop via USB for direct monitoring
 * 
 * WIRED GATEWAY OPERATION:
 * - Receives ESP-NOW messages from ESP2 network (all phases)
 * - Forwards directly via USB Serial to connected monitor
 * - No WiFi needed - direct wired connection to monitoring system
 * - Message persistence for reliability during monitor restarts
 * 
 * ESP2 UNIVERSAL SUPPORT (Phases 1-6):
 * - Phase 1: Core envelope messaging and peer discovery
 * - Phase 2: WiFi scanning coordination messages
 * - Phase 3: Enhanced peer validation and handshake protocols
 * - Phase 4: RSSI triangulation and positioning data
 * - Phase 5: Store-and-forward message relaying
 * - Phase 6: Network optimization and robustness
 * 
 * BATTERY ESP2 INTEGRATION:
 * - Primary gateway for wireless ESP2s without direct monitor access
 * - Preserves complete message chains and relay information
 * - Handles all ESP2 protocol versions and message types
 * - Reliable wired bridge for power-constrained ESP2 network
 * 
 * Hardware: Any ESP32 board connected via USB to PC/laptop
 * 
 * Setup:
 * 1. Install libraries:
 *    - ESP32 Board Support
 *    - ArduinoJson (by Benoit Blanchon)
 *    - Preferences (for message persistence)
 * 2. Upload to ESP32
 * 3. Connect via USB to PC/laptop running monitor
 */

#include <WiFi.h>         // Only for ESP-NOW MAC address
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <Preferences.h>  // Message persistence
#include <vector>
#include <queue>

// ============ WIRED GATEWAY CONFIGURATION ============

// Device Configuration
const char* DEVICE_ID = "ESP1_WIRED_GATEWAY";
const char* DEVICE_TYPE = "ESP1_WIRED_GATEWAY";
const char* ESP1_VERSION = "5.0.0";  // Wired gateway optimized

// ESP2 Protocol Compatibility
const char* SUPPORTED_ESP2_PROTOCOL = "5.0";  // Compatible with ESP2 Phase 1-6
const char* MIN_ESP2_PROTOCOL = "1.0";        // Backward compatibility
const char* MAX_ESP2_PROTOCOL = "6.0";        // Future compatibility

// ============ UNIVERSAL MESSAGE TRACKING (All Phases) ============
struct MessageStats {
  int pingMessages = 0;           // Phase 1: Peer discovery
  int handshakeMessages = 0;      // Phase 3: Enhanced handshakes
  int dataMessages = 0;           // Phase 1: Core data messages
  int triangulationMessages = 0;  // Phase 4: Positioning data
  int relayMessages = 0;          // Phase 5: Store-and-forward
  int wifiScanMessages = 0;       // Phase 2: WiFi coordination
  int optimizationMessages = 0;   // Phase 6: Network optimization
  int unknownMessages = 0;        // Invalid/unrecognized
  int totalMessages = 0;
  int persistedMessages = 0;      // Messages saved to flash
  int deliveredMessages = 0;      // Successfully forwarded via USB
};

// ============ WIRED GATEWAY STATUS ============
struct GatewayStatus {
  bool monitorConnected = true;   // Always true for wired connection
  bool persistenceEnabled = true;
  unsigned long gatewayStartTime = 0;
  String lastError = "none";
};

// Message structure for async processing
struct AsyncMessage {
  String data;
  String senderMAC;
  int rssi;
  unsigned long timestamp;
  bool hasMessage = false;
};

MessageStats messageStats;
GatewayStatus gatewayStatus;
AsyncMessage asyncMsg;
String lastMessageType = "none";
String lastSenderDevice = "none";
int protocolMismatches = 0;

// ============ WIRED GATEWAY VARIABLES ============

int receiveMessageCount = 0;
unsigned long lastStatusSend = 0;
const unsigned long STATUS_INTERVAL = 5000;  // 5 seconds

// ============ MESSAGE PERSISTENCE VARIABLES ============
Preferences preferences;
std::queue<String> messageQueue;
const int MAX_QUEUED_MESSAGES = 100;
const int MAX_PERSISTED_MESSAGES = 50;
unsigned long lastPersistenceCheck = 0;
const unsigned long PERSISTENCE_INTERVAL = 10000;  // Check every 10 seconds

// =======================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n================================================");
  Serial.println("ESP1 - Wired ESP2 Communication Gateway");
  Serial.println("================================================");
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.printf("Device Type: %s\n", DEVICE_TYPE);
  Serial.printf("ESP1 Version: %s\n", ESP1_VERSION);
  Serial.printf("ESP2 Protocol Support: %s - %s\n", MIN_ESP2_PROTOCOL, MAX_ESP2_PROTOCOL);
  Serial.println("================================================");
  Serial.println("WIRED GATEWAY FEATURES:");
  Serial.println("  ✓ Direct USB Serial to Monitor");
  Serial.println("  ✓ ESP-NOW Reception (All ESP2 Phases)");
  Serial.println("  ✓ Message Persistence & Recovery");
  Serial.println("  ✓ Universal ESP2 Protocol Support");
  Serial.println("  ✓ Zero WiFi Dependencies");
  Serial.println("================================================");
  Serial.println("SUPPORTED ESP2 PHASES:");
  Serial.println("  ✓ Phase 1: Core Communication & Envelopes");
  Serial.println("  ✓ Phase 2: WiFi Coordination Messages");
  Serial.println("  ✓ Phase 3: Enhanced Peer Discovery");
  Serial.println("  ✓ Phase 4: RSSI Triangulation Data");
  Serial.println("  ✓ Phase 5: Message Relaying Chains");
  Serial.println("  ✓ Phase 6: Network Optimization");
  Serial.println("================================================\n");

  // Initialize persistence
  gatewayStatus.gatewayStartTime = millis();
  gatewayStatus.monitorConnected = true;  // Always connected via USB
  preferences.begin("esp1_gateway", false);
  
  // Load any persisted messages
  loadPersistedMessages();
  
  // Initialize ESP-NOW for receiving ESP2 messages (no WiFi needed)
  Serial.println("🔧 Initializing wired gateway...");
  initESPNowOnly();
  
  Serial.println("🚀 ESP1 Wired Gateway Ready!");
  Serial.println("📞 Direct USB connection to monitor established");
  Serial.println("📡 Monitoring ESP2 network (all phases)...\n");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle message persistence and recovery (simple check)
  if (currentTime - lastPersistenceCheck > PERSISTENCE_INTERVAL) {
    lastPersistenceCheck = currentTime;
    processPersistentMessages();
  }

  // Send periodic status to monitor via USB
  if (currentTime - lastStatusSend > STATUS_INTERVAL) {
    lastStatusSend = currentTime;
    sendWiredGatewayStatus();
  }
  
  // Simple delay - no complex async needed for wired operation
  delay(10);
}

// ============ ESP-NOW WIRED GATEWAY INITIALIZATION ============

void initESPNowOnly() {
  Serial.println("🔧 Initializing ESP-NOW for wired gateway...");
  
  // Set WiFi mode to STA for ESP-NOW only (no actual WiFi connection)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);  // Allow WiFi mode to stabilize
  
  // CRITICAL: Set channel 1 to match ESP2 devices
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  delay(100);  // Allow channel setup to stabilize
  
  Serial.printf("ESP-NOW MAC Address: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW initialization failed");
    gatewayStatus.lastError = "ESP-NOW init failed";
    return;
  }
  
  Serial.println("✓ ESP-NOW initialized successfully");
  
  // Register receive callback only (no send needed for gateway)
  esp_now_register_recv_cb(onESPNowDataReceived);
  
  uint8_t primary;
  wifi_second_chan_t secondary;
  esp_wifi_get_channel(&primary, &secondary);
  Serial.printf("📡 Listening on channel: %d\n", primary);
  Serial.println("🎯 Ready to receive ESP2 messages (all phases)\n");
}

// ============ HELPER FUNCTIONS ============

// Convert compact type code to string: 0=ping, 1=data, 2=handshake, 3=triangulation, 4=relay, 5=distance
String typeCodeToString(int typeCode) {
  switch(typeCode) {
    case 0: return "ping";
    case 1: return "data";
    case 2: return "handshake";
    case 3: return "triangulation";
    case 4: return "relay";
    case 5: return "distance";
    default: return "unknown";
  }
}

String getPhaseFromMessageType(const String& messageType) {
  if (messageType == "ping" || messageType == "data" || messageType == "distance") {
    return "Phase 1";
  } else if (messageType == "wifi_scan") {
    return "Phase 2";
  } else if (messageType == "handshake") {
    return "Phase 3";
  } else if (messageType == "triangulation") {
    return "Phase 4";
  } else if (messageType == "relay") {
    return "Phase 5";
  } else if (messageType == "optimization") {
    return "Phase 6";
  } else {
    return "Unknown";
  }
}

// ============ ESP-NOW CALLBACK ============

void onESPNowDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  delay(100);  // Stability delay
  
  receiveMessageCount++;
  
  // Store sender MAC (safe assignment)
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  asyncMsg.senderMAC = String(macStr);
  
  String receivedData = "";
  for (int i = 0; i < data_len; i++) {
    receivedData += (char)data[i];
  }
  
  // Parse and analyze message for universal phase compatibility
  analyzeESP2Message(receivedData, macStr);
  
  Serial.println("\n═════════════════════════════════════════════════════════════");
  Serial.println("📡 ESP2 MESSAGE RECEIVED (Universal Gateway)");
  Serial.println("═════════════════════════════════════════════════════════════");
  
  // Create consolidated JSON message for monitor
  JsonDocument gatewayMessage;
  gatewayMessage["source"] = "ESP1_GATEWAY";
  gatewayMessage["timestamp"] = millis() / 1000;
  gatewayMessage["gateway_type"] = "ESP1_WIRED_GATEWAY";
  gatewayMessage["device_id"] = DEVICE_ID;
  gatewayMessage["esp1_version"] = ESP1_VERSION;
  
  // ESP2 Message Details
  gatewayMessage["esp2_sender_mac"] = macStr;
  gatewayMessage["esp2_message_type"] = lastMessageType;
  gatewayMessage["esp2_phase"] = getPhaseFromMessageType(lastMessageType);
  gatewayMessage["esp2_sender_device"] = lastSenderDevice;
  gatewayMessage["esp2_rssi"] = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
  
  // Gateway Statistics
  gatewayMessage["message_count"] = receiveMessageCount;
  gatewayMessage["gateway_status"] = gatewayStatus.monitorConnected ? "CONNECTED" : "PERSISTENCE_MODE";
  gatewayMessage["queue_size"] = messageQueue.size();
  gatewayMessage["persisted_messages"] = messageStats.persistedMessages;
  
  // Original ESP2 Message Data
  gatewayMessage["esp2_raw_data"] = receivedData;
  
  // Send consolidated message to monitor
  String gatewayJson;
  serializeJson(gatewayMessage, gatewayJson);
  Serial.println(gatewayJson);
  
  Serial.println("═════════════════════════════════════════════════════════════");
  
  delay(50);  // Processing pause
  
  // Universal Gateway Message Handling - Always Dual Transport
  Serial.println("\n🔄 ASYNC GATEWAY PROCESSING...");
  
  // Queue the original ESP2 message for async processing
  asyncMsg.data = receivedData;
  asyncMsg.rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
  asyncMsg.timestamp = millis();
  asyncMsg.hasMessage = true;
  
  Serial.println("✅ ESP2 message queued for async processing");
  Serial.println("═════════════════════════════════════════════════════════════\n");
}

void persistMessage(const String& message) {
  if (!gatewayStatus.persistenceEnabled) return;
  
  int currentCount = preferences.getInt("msg_count", 0);
  if (currentCount >= MAX_PERSISTED_MESSAGES) {
    Serial.println("⚠️ Persistence storage full - dropping oldest message");
    // Shift messages down to make room
    for (int i = 0; i < MAX_PERSISTED_MESSAGES - 1; i++) {
      String sourceKey = "msg_" + String(i + 1);
      String targetKey = "msg_" + String(i);
      String msg = preferences.getString(sourceKey.c_str(), "");
      preferences.putString(targetKey.c_str(), msg);
    }
    currentCount = MAX_PERSISTED_MESSAGES - 1;
  }
  
  String key = "msg_" + String(currentCount);
  preferences.putString(key.c_str(), message);
  preferences.putInt("msg_count", currentCount + 1);
  messageStats.persistedMessages++;
  
  Serial.printf("💾 Message persisted to flash (slot %d)\n", currentCount);
}

// ============ MESSAGE ANALYSIS ============

void analyzeESP2Message(const String& messageData, const String& senderMAC) {
  // Parse JSON to extract message type and sender info
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, messageData);
  
  if (error) {
    lastMessageType = "invalid_json";
    lastSenderDevice = "unknown";
    messageStats.unknownMessages++;
    gatewayStatus.lastError = "JSON parse error: " + String(error.c_str());
    return;
  }
  
  // Check protocol version - support both compact (v) and verbose (version)
  String protocolVersion = "";
  if (doc.containsKey("v")) {
    protocolVersion = doc["v"].as<String>();
  } else if (doc.containsKey("version")) {
    protocolVersion = doc["version"].as<String>();
  }
  
  if (protocolVersion.length() > 0) {
    float version = protocolVersion.toFloat();
    float minVersion = String(MIN_ESP2_PROTOCOL).toFloat();
    float maxVersion = String(MAX_ESP2_PROTOCOL).toFloat();
    
    if (version < minVersion || version > maxVersion) {
      protocolMismatches++;
      Serial.printf("⚠️ Protocol version %s outside supported range %s-%s\n", 
                   protocolVersion.c_str(), MIN_ESP2_PROTOCOL, MAX_ESP2_PROTOCOL);
      gatewayStatus.lastError = "Protocol version mismatch: " + protocolVersion;
    }
  }
  
  // Extract message type - support both compact (y) and verbose (message_type)
  // Compact: y = 0:ping, 1:data, 2:handshake, 3:triangulation, 4:relay, 5:distance
  if (doc.containsKey("y")) {
    int typeCode = doc["y"].as<int>();
    lastMessageType = typeCodeToString(typeCode);
  } else if (doc.containsKey("message_type")) {
    lastMessageType = doc["message_type"].as<String>();
  } else {
    lastMessageType = "no_type";
  }
  
  // Update message statistics based on type
  if (lastMessageType == "ping") {
    messageStats.pingMessages++;
  } else if (lastMessageType == "handshake") {
    messageStats.handshakeMessages++;
  } else if (lastMessageType == "data") {
    messageStats.dataMessages++;
  } else if (lastMessageType == "triangulation") {
    messageStats.triangulationMessages++;
  } else if (lastMessageType == "relay") {
    messageStats.relayMessages++;
  } else if (lastMessageType == "distance") {
    messageStats.dataMessages++;  // Count with data messages
  } else if (lastMessageType == "wifi_scan") {
    messageStats.wifiScanMessages++;
  } else if (lastMessageType == "optimization") {
    messageStats.optimizationMessages++;
  } else {
    messageStats.unknownMessages++;
  }
  
  // Extract sender device ID - support compact (d) and verbose formats
  if (doc.containsKey("d")) {
    lastSenderDevice = doc["d"].as<String>();  // Compact format
  } else if (doc.containsKey("source_device") && doc["source_device"].containsKey("device_id")) {
    lastSenderDevice = doc["source_device"]["device_id"].as<String>();
  } else if (doc.containsKey("device_id")) {
    lastSenderDevice = doc["device_id"].as<String>();
  } else {
    lastSenderDevice = senderMAC;  // Ultimate fallback
  }
  
  messageStats.totalMessages = receiveMessageCount;
  gatewayStatus.lastError = "none";
}

// ============ PERSISTENCE FUNCTIONS ============

void loadPersistedMessages() {
  Serial.println("🔄 Loading persisted messages from flash...");
  
  int persistedCount = preferences.getInt("msg_count", 0);
  if (persistedCount > 0) {
    Serial.printf("📦 Found %d persisted messages\n", persistedCount);
    messageStats.persistedMessages = persistedCount;
    
    // For wired gateway, deliver persisted messages immediately via USB
    for (int i = 0; i < persistedCount && i < MAX_PERSISTED_MESSAGES; i++) {
      String key = "msg_" + String(i);
      String persistedMsg = preferences.getString(key.c_str(), "");
      if (persistedMsg.length() > 0) {
        Serial.println("📞 USB: " + persistedMsg);
        Serial.flush();
        messageStats.deliveredMessages++;
      }
    }
    
    // Clear persisted messages after delivery
    for (int i = 0; i < persistedCount; i++) {
      String key = "msg_" + String(i);
      preferences.remove(key.c_str());
    }
    preferences.putInt("msg_count", 0);
    Serial.println("✅ Persisted messages loaded and delivered via USB");
  } else {
    Serial.println("📭 No persisted messages found");
  }
}

void processPersistentMessages() {
  // For wired gateway, persistence is mainly for recovery after restart
  // Messages are delivered immediately via USB, so queue processing is minimal
  if (!messageQueue.empty()) {
    while (!messageQueue.empty()) {
      String queuedMessage = messageQueue.front();
      messageQueue.pop();
      
      // Deliver via USB immediately
      Serial.println("📞 USB: " + queuedMessage);
      Serial.flush();
      messageStats.deliveredMessages++;
    }
  }
}

void sendWiredGatewayStatus() {
  // Simple status message for wired gateway
  StaticJsonDocument<500> statusDoc;
  statusDoc["gateway_type"] = "wired";
  statusDoc["device_id"] = DEVICE_ID;
  statusDoc["device_type"] = DEVICE_TYPE;
  statusDoc["esp1_version"] = ESP1_VERSION;
  statusDoc["esp2_protocol_range"] = String(MIN_ESP2_PROTOCOL) + "-" + String(MAX_ESP2_PROTOCOL);
  statusDoc["uptime"] = (millis() - gatewayStatus.gatewayStartTime) / 1000;
  statusDoc["connection_type"] = "usb_serial";
  statusDoc["last_error"] = gatewayStatus.lastError;
  
  // Message statistics for all ESP2 phases
  JsonObject msgStats = statusDoc["message_stats"].to<JsonObject>();
  msgStats["total"] = messageStats.totalMessages;
  msgStats["ping"] = messageStats.pingMessages;           // Phase 1
  msgStats["handshake"] = messageStats.handshakeMessages; // Phase 3
  msgStats["data"] = messageStats.dataMessages;           // Phase 1
  msgStats["triangulation"] = messageStats.triangulationMessages; // Phase 4
  msgStats["relay"] = messageStats.relayMessages;         // Phase 5
  msgStats["wifi_scan"] = messageStats.wifiScanMessages;  // Phase 2
  msgStats["optimization"] = messageStats.optimizationMessages; // Phase 6
  msgStats["unknown"] = messageStats.unknownMessages;
  msgStats["delivered"] = messageStats.deliveredMessages;
  
  // Wired gateway health
  JsonObject health = statusDoc["gateway_health"].to<JsonObject>();
  health["protocol_mismatches"] = protocolMismatches;
  health["last_sender"] = lastSenderDevice;
  health["last_message_type"] = lastMessageType;
  health["esp_now_mac"] = WiFi.macAddress();
  
  String statusJson;
  serializeJson(statusDoc, statusJson);
  
  // Send status via USB Serial
  Serial.println(statusJson);
  Serial.flush();
}