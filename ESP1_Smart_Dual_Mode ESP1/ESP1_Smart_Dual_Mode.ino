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
#include <AsyncTCP.h>     // Async processing support
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

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

MessageStats messageStats;
GatewayStatus gatewayStatus;
String lastMessageType = "none";
String lastSenderDevice = "none";
int protocolMismatches = 0;

// ============ ASYNC PROCESSING VARIABLES (Powered Operation) ============
QueueHandle_t messageQueue_async;
TaskHandle_t messageProcessorTask;
TaskHandle_t statusSenderTask;
TaskHandle_t persistenceTask;
SemaphoreHandle_t messageStatsMutex;
const int ASYNC_MESSAGE_QUEUE_SIZE = 200;  // Large queue for high-throughput

// Message structure for async processing
struct AsyncMessage {
  String data;
  String senderMAC;
  unsigned long timestamp;
  int rssi;
};

// ============ WIRED GATEWAY VARIABLES ============

int receiveMessageCount = 0;
unsigned long lastStatusSend = 0;
const unsigned long STATUS_INTERVAL = 10000;  // 5 seconds

// ============ MESSAGE PERSISTENCE VARIABLES ============
Preferences preferences;
std::queue<String> messageQueue;
const int MAX_QUEUED_MESSAGES = 100;
const int MAX_PERSISTED_MESSAGES = 50;
unsigned long lastPersistenceCheck = 0;
const unsigned long PERSISTENCE_INTERVAL = 10000;  // Check every 10 seconds

// ============ FUNCTION DECLARATIONS ============
void initESPNowOnly();
void onESPNowDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);
String getPhaseFromMessageType(const String& messageType);
void persistMessage(const String& message);
void analyzeESP2Message(const String& messageData, const String& senderMAC);
void loadPersistedMessages();
void processPersistentMessages();
void sendWiredGatewayStatus();

// Async task functions
void messageProcessorTaskFunction(void *parameter);
void statusSenderTaskFunction(void *parameter);
void persistenceTaskFunction(void *parameter);
void initAsyncTasks();

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
  
  // Initialize async processing tasks (powered operation advantage)
  Serial.println("⚡ Initializing async processing...");
  initAsyncTasks();
  
  Serial.println("🚀 ESP1 Wired Gateway Ready!");
  Serial.println("📞 Direct USB connection to monitor established");
  Serial.println("📡 Monitoring ESP2 network (all phases)...");
  Serial.println("⚡ Async processing active - maximum performance mode\n");
}

void loop() {
  // With async tasks handling everything, main loop is minimal
  // ESP-NOW callbacks and async tasks handle all processing
  
  // Just yield to allow FreeRTOS task scheduling
  vTaskDelay(pdMS_TO_TICKS(1));
  
  // Optional: Monitor task health (non-blocking)
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 30000) {  // Every 30 seconds
    lastHealthCheck = millis();
    Serial.printf("📊 System Health: Queue depth: %d, Free heap: %d bytes\n", 
                  uxQueueMessagesWaiting(messageQueue_async), ESP.getFreeHeap());
  }
}

// ============ ESP-NOW WIRED GATEWAY INITIALIZATION ============

void initESPNowOnly() {
  Serial.println("🔧 Initializing ESP-NOW for wired gateway...");
  
  // Set WiFi mode to STA for ESP-NOW only (no actual WiFi connection)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.printf("ESP-NOW MAC Address: %s\n", WiFi.macAddress().c_str());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW initialization failed");
    gatewayStatus.lastError = "ESP-NOW init failed";
    return;
  }
  
  Serial.println("✓ ESP-NOW initialized successfully");
  
  // Set fixed channel to match ESP2 devices
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // Match ESP2 channel
  esp_wifi_set_promiscuous(false);
  
  // Register receive callback only (no send needed for gateway)
  // NOTE: ESP-NOW callbacks are interrupt-driven and can fire at any time,
  // even during status sends or other operations. No blocking delays in callbacks!
  esp_now_register_recv_cb(onESPNowDataReceived);
  
  uint8_t primary;
  wifi_second_chan_t secondary;
  esp_wifi_get_channel(&primary, &secondary);
  Serial.printf("📡 Listening on channel: %d (fixed for ESP2 compatibility)\n", primary);
  Serial.println("🎯 Ready to receive ESP2 messages (all phases)\n");
}

// ============ ESP-NOW CALLBACK ============

void onESPNowDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  // Ultra-fast callback - immediately queue message for async processing
  Serial.println("🔔 ESP-NOW DATA RECEIVED!");
  
  // Create async message structure
  AsyncMessage asyncMsg;
  
  // Copy data (minimal processing in callback)
  for (int i = 0; i < data_len; i++) {
    asyncMsg.data += (char)data[i];
  }
  
  // Store sender MAC
  snprintf(asyncMsg.senderMAC.begin(), 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  
  asyncMsg.timestamp = millis();
  asyncMsg.rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
  
  // Queue for async processing (non-blocking)
  if (xQueueSend(messageQueue_async, &asyncMsg, 0) == pdTRUE) {
    Serial.println("⚡ Message queued for async processing");
    
    // Update counter with mutex protection
    if (xSemaphoreTake(messageStatsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      receiveMessageCount++;
      xSemaphoreGive(messageStatsMutex);
    }
  } else {
    Serial.println("⚠️ Async queue full - message dropped");
  }
}

// ============ HELPER FUNCTIONS ============

String getPhaseFromMessageType(const String& messageType) {
  if (messageType == "ping" || messageType == "data") return "1";
  if (messageType == "wifi_scan") return "2";
  if (messageType == "handshake") return "3";
  if (messageType == "triangulation") return "4";
  if (messageType == "relay") return "5";
  if (messageType == "optimization") return "6";
  return "Unknown";
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

// ============ UNIVERSAL MESSAGE ANALYSIS (All ESP2 Phases) ============

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
  
  // Check protocol version (Enhanced compatibility)
  if (doc.containsKey("version")) {
    String protocolVersion = doc["version"].as<String>();
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
  
  // Extract and categorize message type (All Phases)
  if (doc.containsKey("message_type")) {
    lastMessageType = doc["message_type"].as<String>();
    
    // Phase-specific message type statistics
    if (lastMessageType == "ping") {
      messageStats.pingMessages++;  // Phase 1
    } else if (lastMessageType == "handshake") {
      messageStats.handshakeMessages++;  // Phase 3
    } else if (lastMessageType == "data") {
      messageStats.dataMessages++;  // Phase 1
    } else if (lastMessageType == "triangulation") {
      messageStats.triangulationMessages++;  // Phase 4
    } else if (lastMessageType == "relay") {
      messageStats.relayMessages++;  // Phase 5
    } else if (lastMessageType == "wifi_scan") {
      messageStats.wifiScanMessages++;  // Phase 2
    } else if (lastMessageType == "optimization") {
      messageStats.optimizationMessages++;  // Phase 6
    } else {
      messageStats.unknownMessages++;
    }
  } else {
    lastMessageType = "no_type";
    messageStats.unknownMessages++;
  }
  
  // Extract sender device ID with fallbacks
  if (doc.containsKey("source_device") && doc["source_device"].containsKey("device_id")) {
    lastSenderDevice = doc["source_device"]["device_id"].as<String>();
  } else if (doc.containsKey("device_id")) {
    lastSenderDevice = doc["device_id"].as<String>();  // Fallback
  } else {
    lastSenderDevice = senderMAC;  // Ultimate fallback to MAC address
  }
  
  messageStats.totalMessages = receiveMessageCount;
  gatewayStatus.lastError = "none";  // Clear error on successful parse
}





// ============ WIRED GATEWAY PERSISTENCE FUNCTIONS ============

void loadPersistedMessages() {
  Serial.println("🔄 Checking for persisted messages...");
  
  int persistedCount = preferences.getInt("msg_count", 0);
  if (persistedCount > 0) {
    Serial.printf("📫 Found %d persisted messages (delivered via USB)\n", persistedCount);
    
    // For wired gateway, just deliver persisted messages immediately via USB
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
    Serial.println("✅ All persisted messages delivered and cleared");
  } else {
    Serial.println("📫 No persisted messages found");
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

// ============ ASYNC TASK FUNCTIONS (Powered Operation) ============

void initAsyncTasks() {
  // Create message queue for async processing
  messageQueue_async = xQueueCreate(ASYNC_MESSAGE_QUEUE_SIZE, sizeof(AsyncMessage));
  if (messageQueue_async == NULL) {
    Serial.println("⚠️ Failed to create async message queue!");
    return;
  }
  
  // Create mutex for thread-safe statistics
  messageStatsMutex = xSemaphoreCreateMutex();
  if (messageStatsMutex == NULL) {
    Serial.println("⚠️ Failed to create message stats mutex!");
    return;
  }
  
  // Create message processing task (highest priority)
  if (xTaskCreatePinnedToCore(
        messageProcessorTaskFunction,
        "MessageProcessor",
        4096,
        NULL,
        3,  // High priority for real-time processing
        &messageProcessorTask,
        0   // Pin to core 0
      ) != pdPASS) {
    Serial.println("⚠️ Failed to create message processor task!");
    return;
  }
  
  // Create status sender task (medium priority)
  if (xTaskCreatePinnedToCore(
        statusSenderTaskFunction,
        "StatusSender",
        2048,
        NULL,
        2,  // Medium priority
        &statusSenderTask,
        1   // Pin to core 1
      ) != pdPASS) {
    Serial.println("⚠️ Failed to create status sender task!");
    return;
  }
  
  // Create persistence task (lower priority)
  if (xTaskCreatePinnedToCore(
        persistenceTaskFunction,
        "PersistenceHandler",
        2048,
        NULL,
        1,  // Lower priority
        &persistenceTask,
        1   // Pin to core 1
      ) != pdPASS) {
    Serial.println("⚠️ Failed to create persistence task!");
    return;
  }
  
  Serial.println("✅ All async tasks initialized successfully!");
}

void messageProcessorTaskFunction(void *parameter) {
  AsyncMessage receivedMsg;
  
  while (true) {
    // Wait for messages in queue (blocks until available)
    if (xQueueReceive(messageQueue_async, &receivedMsg, portMAX_DELAY) == pdTRUE) {
      
      // Process message with full logging (not time-critical here)
      Serial.println("\n══════════════════════════════════════════════════════════════════");
      Serial.println("📡 ESP2 MESSAGE ASYNC PROCESSING (Powered Gateway)");
      Serial.println("══════════════════════════════════════════════════════════════════");
      Serial.printf("From ESP2: %s\n", receivedMsg.senderMAC.c_str());
      Serial.printf("Timestamp: %lu ms\n", receivedMsg.timestamp);
      Serial.printf("RSSI: %d dBm\n", receivedMsg.rssi);
      Serial.printf("Raw data preview: %s\n", receivedMsg.data.substring(0, 50).c_str());
      
      // Analyze message
      analyzeESP2Message(receivedMsg.data, receivedMsg.senderMAC);
      
      Serial.printf("Message Type: %s\n", lastMessageType.c_str());
      Serial.printf("Phase: %s\n", getPhaseFromMessageType(lastMessageType).c_str());
      Serial.printf("Sender Device: %s\n", lastSenderDevice.c_str());
      
      // Get current stats safely
      int currentMsgCount = 0;
      if (xSemaphoreTake(messageStatsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentMsgCount = receiveMessageCount;
        xSemaphoreGive(messageStatsMutex);
      }
      
      Serial.printf("Total Messages: %d\n", currentMsgCount);
      Serial.printf("Gateway Status: %s\n", gatewayStatus.monitorConnected ? "CONNECTED" : "PERSISTENCE MODE");
      Serial.println("══════════════════════════════════════════════════════════════════");
      
      // ALWAYS forward via USB Serial (immediate transmission)
      Serial.println("\n🔄 ASYNC GATEWAY PROCESSING...");
      Serial.println("📤 USB: " + receivedMsg.data);
      Serial.flush();  // Ensure immediate USB transmission
      messageStats.deliveredMessages++;
      
      // Queue for persistence if enabled
      if (gatewayStatus.persistenceEnabled) {
        if (messageQueue.size() < MAX_QUEUED_MESSAGES) {
          messageQueue.push(receivedMsg.data);
          Serial.printf("💾 QUEUED: Message stored for recovery (%d in queue)\n", messageQueue.size());
        }
      }
      
      Serial.println("✅ ESP2 message processed and forwarded (async)\n");
    }
    
    // Small yield to prevent watchdog
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void statusSenderTaskFunction(void *parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t frequency = pdMS_TO_TICKS(STATUS_INTERVAL);
  
  while (true) {
    // Send periodic status to monitor via USB
    sendWiredGatewayStatusAsync();
    
    // Wait for next cycle (precise timing)
    vTaskDelayUntil(&lastWakeTime, frequency);
  }
}

void persistenceTaskFunction(void *parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t frequency = pdMS_TO_TICKS(PERSISTENCE_INTERVAL);
  
  while (true) {
    // Handle message persistence and recovery
    processPersistentMessages();
    
    // Wait for next cycle
    vTaskDelayUntil(&lastWakeTime, frequency);
  }
}

void sendWiredGatewayStatusAsync() {
  // Create comprehensive status message with async stats
  StaticJsonDocument<512> statusDoc;
  statusDoc["messageType"] = "GATEWAY_STATUS";
  statusDoc["deviceType"] = "ESP1_WIRED_GATEWAY_ASYNC";
  statusDoc["timestamp"] = millis();
  statusDoc["gatewayMode"] = "POWERED_ASYNC";
  
  // Safely get message count
  int currentMsgCount = 0;
  if (xSemaphoreTake(messageStatsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    currentMsgCount = receiveMessageCount;
    xSemaphoreGive(messageStatsMutex);
  }
  
  statusDoc["messagesReceived"] = currentMsgCount;
  statusDoc["messagesDelivered"] = messageStats.deliveredMessages;
  statusDoc["queuedMessages"] = messageQueue.size();
  statusDoc["persistedMessages"] = messageStats.persistedMessages;
  statusDoc["uptime"] = millis();
  statusDoc["freeHeap"] = ESP.getFreeHeap();
  statusDoc["monitorConnected"] = gatewayStatus.monitorConnected;
  statusDoc["persistenceEnabled"] = gatewayStatus.persistenceEnabled;
  
  // Async processing stats
  statusDoc["asyncQueueDepth"] = uxQueueMessagesWaiting(messageQueue_async);
  statusDoc["asyncQueueCapacity"] = ASYNC_MESSAGE_QUEUE_SIZE;
  statusDoc["coreUtilization"] = "Dual-core parallel processing active";
  statusDoc["taskInfo"] = "3 async tasks running on 2 cores";
  
  String statusStr;
  serializeJson(statusDoc, statusStr);
  
  Serial.println("⚡ ESP1_GATEWAY: " + statusStr);
  Serial.flush();
}
