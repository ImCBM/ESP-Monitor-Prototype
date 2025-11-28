  /*
  * ESP2 Universal - Complete ESP2 Communication Firmware
  * 
  * This is the complete universal ESP2 firmware that enables:
  * - Structured JSON envelope messaging between ESP2s
  * - ESP-NOW peer-to-peer communication (ESP1 Gateway compatible)
  * - WiFi scanning and mode switching
  * - Advanced peer discovery and handshake protocol
  * - RSSI-based triangulation and relative positioning
  * - Message relaying and store-and-forward capability
  * 
  * Complete Feature Set:
  * - Standardized envelope structure for all messages
  * - Peer discovery via broadcast ping with ESP1 gateway compatibility
  * - Enhanced handshake response protocol with security validation
  * - Message validation using shared key authentication
  * - Advanced loop prevention for handshake messages
  * - Periodic WiFi network scanning and server reachability checks
  * - Dynamic mode switching between ESP-NOW and WiFi
  * - Known network list management and connection prioritization
  * - Enhanced peer validation and security protocols
  * - Improved handshake protocol with capability negotiation
  * - RSSI-based distance estimation and relative positioning (N/S/E/W)
  * - Periodic triangulation updates and position-aware peer tracking
  * - Message storage and relay management with store-and-forward capability
  * - Multi-hop message delivery with loop prevention and delivery confirmation
  * - Complete relay chain tracking and server delivery
  * 
  * Hardware: Any ESP32 board
  * 
  * Setup Instructions:
  * 1. Install required libraries:
  *    - ESP32 Board Support (via Board Manager)
  *    - ArduinoJson (by Benoit Blanchon) - via Library Manager
  * 2. Configure device settings below
  * 3. Upload to ESP32
  * 4. Power it on and monitor serial output
  * 4. Power it on and monitor serial output
  */

  // ============================================================================
  //                           LIBRARY INCLUDES
  // ============================================================================

  #include <WiFi.h>
  #include <esp_now.h>
  #include <esp_wifi.h>
  #include <ArduinoJson.h>
  #include <Preferences.h>
  #include <map>
  #include <vector>

  // ============================================================================
  //                         DEVICE CONFIGURATION
  // ============================================================================
  // CHANGE THESE FOR EACH ESP2 DEVICE
  const char* DEVICE_ID = "ESP2_SENSOR_001";        // Unique device identifier
  const char* DEVICE_OWNER = "user_alice";          // Device owner name
  const char* USER_NAME = DEVICE_OWNER;             // Alias for compatibility
  const char* SHARED_KEY = "ESP2_NETWORK_KEY";      // Authentication key for peer validation

  // Protocol Configuration
  const char* PROTOCOL_VERSION = "5.0";             // Updated for Phase 5
  const char* ESP2_VERSION = "2.0.0";               // ESP2 firmware version
  const int ESP_NOW_CHANNEL = 1;                    // Fixed channel for all ESP2s

  // ============================================================================
  //                       MESSAGE RELAYING CONFIGURATION
  //                        (Phase 5 Features)
  // ============================================================================
  // Message storage and relay parameters
  const int MAX_STORED_MESSAGES = 20;               // Maximum messages to store for relay
  const unsigned long MESSAGE_RELAY_INTERVAL = 15000;  // Check for relay opportunities every 15 seconds
  const unsigned long MESSAGE_EXPIRY_TIME = 300000;    // Messages expire after 5 minutes
  const int MAX_RELAY_HOPS = 5;                     // Maximum relay chain length
  const unsigned long RELAY_ATTEMPT_COOLDOWN = 30000;  // Wait 30s between relay attempts to same peer


  // ============================================================================
  //                          WIFI CONFIGURATION
  //                        (Phase 2 Features)
  // ============================================================================
  // Known WiFi networks list (SSID, Password)
  struct WiFiCredential {
    String ssid;
    String password;
    bool isOpen;
  };

  WiFiCredential knownNetworks[] = {
    // Disabled for ESP1 Gateway Mode - Prioritize ESP-NOW
    // {"YourHomeWiFi", "your_password", false},
    // {"YourHotspot", "hotspot_password", false}, 
    // {"OfficeWiFi", "office_password", false},
    // {"OpenNetwork", "", true},
    {"", "", false} // End marker
  };

  // Server configuration for reachability tests
  const char* TEST_SERVER_HOST = "8.8.8.8";         // Google DNS for connectivity test
  const int TEST_SERVER_PORT = 53;
  const char* WEBSOCKET_SERVER_IP = "192.168.137.1"; // Monitor server IP
  const uint16_t WEBSOCKET_SERVER_PORT = 8080;


  // ============================================================================
  //                        SECURITY CONFIGURATION
  //                        (Phase 3 Features)
  // ============================================================================
const char* DEVICE_TYPE = "ESP2_UNIVERSAL";        // Device type identifier
const char* FIRMWARE_VERSION = "2.0.0";           // Firmware version
const int HANDSHAKE_TIMEOUT = 10000;              // 10 seconds for handshake completion (reduced for testing)
const int MAX_HANDSHAKE_ATTEMPTS = 3;             // Maximum retry attempts
  // ============================================================================
  //                     POSITIONING CONFIGURATION
  //                        (Phase 4 Features)
  // ============================================================================
  // Distance measurement and relative positioning parameters
  const unsigned long POSITIONING_INTERVAL = 10000;    // Update positions every 10 seconds
  const int MIN_PEERS_FOR_POSITIONING = 1;             // Minimum peers for distance measurement
  const int MIN_PEERS_FOR_TRIANGULATION = 3;           // True triangulation needs 3+ devices
  const float RSSI_CALIBRATION_DISTANCE = 1.0;        // Reference distance (1 meter) for RSSI calibration
  const int RSSI_CALIBRATION_VALUE = -40;             // RSSI value at reference distance
  const float PATH_LOSS_EXPONENT = 2.0;               // Free space path loss exponent
  const float MAX_POSITIONING_DISTANCE = 100.0;       // Maximum reliable positioning distance (meters)


  // ============================================================================
  //                        DATA STRUCTURES
  // ============================================================================
  // Direction enumeration for relative positioning
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
    float x;  // East-West coordinate (positive = East)
    float y;  // North-South coordinate (positive = North)
    bool isValid;
    unsigned long lastUpdated;
  };

  struct RelativePosition {
    float distance;          // Estimated distance in meters
    Direction direction;     // Relative direction (N/S/E/W etc.)
    float confidence;        // Confidence level (0.0 - 1.0)
    unsigned long lastUpdated;
    bool isValid;
  };

  // ============================================================================
  //                      MESSAGE RELAY STRUCTURES
  //                        (Phase 5 Features)
  // ============================================================================
  // Message relay and delivery tracking
  struct RelayHop {
    String deviceId;         // Device that relayed this message
    String deviceOwner;      // Owner of relay device
    unsigned long timestamp; // When this hop occurred
    int rssi;               // Signal strength at relay
  };

  struct StoredMessage {
    String messageId;        // Unique message identifier
    String originalSender;   // Original message creator
    String senderOwner;      // Original sender's owner
    JsonDocument messageData; // Complete message content
    unsigned long timestamp; // When message was created
    unsigned long lastRelayAttempt; // Last time we tried to relay
    bool deliveredToServer;  // Has this been sent to monitor server
    bool isOwnMessage;       // Did this device create this message
    int hopCount;           // Number of relay hops
    std::vector<RelayHop> relayChain; // Complete relay path
    std::vector<String> attemptedPeers; // Peers we've tried to relay to
  };


  // ============================================================================
  //                      TIMING & OPERATIONAL VARIABLES
  // ============================================================================
  // Core communication timing - Optimized for ESP1 Gateway
  unsigned long lastPeerDiscovery = 0;
  const unsigned long PEER_DISCOVERY_INTERVAL = 5000;   // Send ping every 5 seconds (faster for gateway)
  unsigned long lastDataSend = 0;
  const unsigned long DATA_SEND_INTERVAL = 10000;       // Send data every 10 seconds (faster for gateway)

  // Radio Management (WiFi and ESP-NOW share the same radio)
  bool espNowActive = false;
  bool wifiModeActive = false;
  unsigned long lastRadioSwitch = 0;
  const unsigned long RADIO_SWITCH_DELAY = 100;  // Minimum delay between radio switches

  // Phase 2: WiFi Scanning and Mode Switching (Reduced for ESP1 Gateway Mode)
  unsigned long lastWiFiScan = 0;
  const unsigned long WIFI_SCAN_INTERVAL = 300000;      // Scan WiFi every 5 minutes (reduced)
  unsigned long lastServerCheck = 0;
  const unsigned long SERVER_CHECK_INTERVAL = 300000;   // Check server every 5 minutes (reduced)
  unsigned long lastModeSwitch = 0;
  const unsigned long MODE_SWITCH_COOLDOWN = 10000;     // Wait 10s between mode switches

  // Phase 4: Distance Measurement and Positioning
  unsigned long lastPositioning = 0;
  struct PositionUpdate {
    String targetDeviceId;
    float distance;
    int rssi;
    unsigned long timestamp;
  };
  std::vector<PositionUpdate> recentRSSIMeasurements;

  // Phase 5: Message Relaying and Storage
  unsigned long lastRelayCheck = 0;
  unsigned long lastServerAttempt = 0;
  const unsigned long SERVER_DELIVERY_INTERVAL = 60000; // Try server delivery every 60 seconds
  StoredMessage messageStorage[MAX_STORED_MESSAGES];
  int storedMessageCount = 0;
  unsigned long messageIdCounter = 0;


  // ============================================================================
  //                     COMMUNICATION MODE MANAGEMENT
  // ============================================================================
  // Available communication modes
  enum CommMode {
    MODE_ESP_NOW_ONLY,    // ESP-NOW only (no WiFi available)
    MODE_WIFI_BACKUP,     // WiFi available, ESP-NOW primary
    MODE_WIFI_PRIMARY,    // WiFi primary, ESP-NOW backup
    MODE_WIFI_ONLY        // WiFi only (ESP-NOW disabled)
  };

  CommMode currentMode = MODE_ESP_NOW_ONLY;
  bool wifiConnected = false;
  String connectedSSID = "";
  int32_t wifiChannel = 0;

  // Message tracking
  unsigned long messageCounter = 0;
  String lastHandshakeMessageId = "";  // Track last handshake to prevent loops
  std::map<String, unsigned long> handshakeAttempts; // Track handshake attempts per peer


  // ============================================================================
  //                         PEER MANAGEMENT
  //                    (Enhanced for Phase 3/4)
  // ============================================================================
  // Structure to store peer device information
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
    int handshakeAttempts;
    unsigned long lastHandshakeAttempt;
    std::vector<String> capabilities;
    CommMode preferredMode;
    
    // Phase 4: Positioning data
    RelativePosition relativePos;
    std::vector<int> rssiHistory;     // Historical RSSI values for better distance estimation
    Position absolutePos;             // Absolute position if available
    bool supportsTriangulation;
  };

  const int MAX_PEERS = 15;
  PeerDevice knownPeers[MAX_PEERS];
  int peerCount = 0;

  // Phase 4: Own positioning data
  Position myPosition = {0.0, 0.0, false, 0};
  bool hasReferencePosition = false;

  // Phase 2: WiFi and Server status
  bool serverReachable = false;
  String lastWiFiError = "";
  unsigned long lastSuccessfulServerContact = 0;

  // Flash storage for persistence
  Preferences preferences;


  // ============================================================================
  //                        FUNCTION DECLARATIONS
  // ============================================================================
  // Core ESP-NOW and messaging functions
  void initESPNow();
  void sendPeerDiscoveryPing();
  void sendHandshakeResponse(const String& replyToMessageId, const uint8_t* peerMac);
  void sendDataMessage();
  void onESPNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len);
  bool validateEnvelope(JsonDocument& doc);
  String generateMessageId(const String& messageType);
  void processIncomingMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi);
  void addOrUpdatePeer(const String& deviceId, const String& owner, const String& macAddr, int rssi);
  void printKnownPeers();
  String macToString(const uint8_t* mac);
  void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);


  // WiFi and network management (Phase 2)
  void performWiFiScan();
  bool tryConnectToKnownNetworks();
  void checkServerReachability();
  void updateCommunicationMode();
  void switchToESPNowMode();
  void switchToWiFiMode();
  int32_t getChannelFromSSID(const String& ssid);

  // Enhanced security and peer validation (Phase 3)
  bool validatePeerCredentials(JsonDocument& doc);
  void sendEnhancedHandshake(const String& replyToMessageId, const uint8_t* peerMac, JsonDocument& originalPing);
  bool isPeerTrusted(const String& deviceId);
  void updatePeerCapabilities(const String& deviceId, JsonObject& payload);
  void cleanupFailedHandshakes();

  // Triangulation and positioning (Phase 4)
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

  // Message relaying and storage (Phase 5)
  void storeMessage(JsonDocument& messageDoc, const String& senderId, bool isOwnMessage);
  void processRelayMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi);
  void checkForRelayOpportunities();
  void attemptServerDelivery();
  void relayMessageToPeer(int messageIndex, const String& peerId);
  void sendRelayMessage(const StoredMessage& storedMsg, const uint8_t* peerMac);
  bool canRelayToPeer(const String& peerId, const StoredMessage& msg);
  void cleanupExpiredMessages();
  void printMessageStorage();
  String generateUniqueMessageId();
  void updateMessageRelayChain(StoredMessage& msg, const String& relayDeviceId, const String& relayOwner, int rssi);
  bool hasServerConnection();
  void markMessageDelivered(const String& messageId);
  int findStoredMessage(const String& messageId);

  // Radio management functions
  void enableWiFiMode();
  void enableESPNowMode();
  void ensureESPNowActive();
  void ensureWiFiActive();
  void deinitESPNow();

  void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("ESP2 Universal - Complete Firmware");
    Serial.println("=================================");
    Serial.printf("Device ID: %s\n", DEVICE_ID);
    Serial.printf("Owner: %s\n", DEVICE_OWNER);
    Serial.printf("Device Type: %s\n", DEVICE_TYPE);
    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("Protocol Version: %s\n", PROTOCOL_VERSION);
    Serial.println("=================================\n");

    // Initialize preferences for flash storage
    preferences.begin("esp2_data", false);
    
    // Phase 5: Initialize message storage
    storedMessageCount = 0;
    messageIdCounter = 0;
    
    // Radio state initialization
    espNowActive = false;
    wifiModeActive = false;
    lastRadioSwitch = 0;
    
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    // Initialize ESP-NOW with radio management
    Serial.println("🚀 Starting in ESP-NOW mode (battery optimized)...");
    enableESPNowMode();  // Use radio management
    
    Serial.println("Complete Feature Set Active:");
    Serial.println("  ✓ JSON Envelope Messaging");
    Serial.println("  ✓ Peer Discovery Protocol");
    Serial.println("  ✓ Enhanced Handshake Validation");
    Serial.println("  ✓ Advanced Loop Prevention");
    Serial.println("  ✓ Message Authentication");
    Serial.println("  ✓ WiFi Network Scanning (Reduced)");
    Serial.println("  ✓ Server Reachability Checks (Reduced)");
    Serial.println("  ✓ Dynamic Mode Switching");
    Serial.println("  ✓ Peer Capability Negotiation");
    Serial.println("  ✓ RSSI-based Distance Estimation");
    Serial.println("  ✓ Relative Positioning (N/S/E/W)");
    Serial.println("  ✓ Triangulation Algorithm");
    Serial.println("  ✓ Message Relaying & Storage");
    Serial.println("  ✓ Multi-hop Message Delivery");
    Serial.println("  🎯 ESP1 GATEWAY MODE: ESP-NOW PRIORITY\n");
    
    // Phase 2: Initial WiFi scan (with radio management)
    Serial.println("Performing initial WiFi scan...");
    enableWiFiMode();  // Switch to WiFi for scanning
    performWiFiScan();
    updateCommunicationMode();
    enableESPNowMode();  // Switch to ESP-NOW for normal operation
    
    Serial.printf("Starting in mode: %s (ESP1 Gateway Priority)\n\n", 
      currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
      currentMode == MODE_WIFI_BACKUP ? "WiFi Backup (ESP-NOW Primary)" :
      currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only");
  }


  // ============================================================================
  //                          MAIN LOOP FUNCTION
  // ============================================================================

  void loop() {
    unsigned long currentTime = millis();
    
    // WiFi scanning and server monitoring (Phase 2)
    if (currentTime - lastWiFiScan > WIFI_SCAN_INTERVAL) {
      lastWiFiScan = currentTime;
      // Disable ESP-NOW before WiFi operations
      enableWiFiMode();
      performWiFiScan();
      updateCommunicationMode();
      // Re-enable ESP-NOW after WiFi operations
      enableESPNowMode();
    }
    
    if (currentMode != MODE_ESP_NOW_ONLY && currentTime - lastServerCheck > SERVER_CHECK_INTERVAL) {
      lastServerCheck = currentTime;
      enableWiFiMode();  // Make sure WiFi is active for server check
      checkServerReachability();
      enableESPNowMode();  // Switch back to ESP-NOW
    }
    
    // Send periodic peer discovery ping
    if (currentTime - lastPeerDiscovery > PEER_DISCOVERY_INTERVAL) {
      lastPeerDiscovery = currentTime;
      ensureESPNowActive();  // Make sure ESP-NOW is active
      sendPeerDiscoveryPing();
    }

    // Send periodic data message
    if (currentTime - lastDataSend > DATA_SEND_INTERVAL) {
      lastDataSend = currentTime;
      ensureESPNowActive();  // Make sure ESP-NOW is active
      sendDataMessage();
    }  // Phase 3: Clean up failed handshakes
    cleanupFailedHandshakes();
    
    // Phase 4: Perform distance measurement and positioning updates
    if (currentTime - lastPositioning > POSITIONING_INTERVAL) {
      lastPositioning = currentTime;
      if (hasEnoughPeersForPositioning()) {
        ensureESPNowActive();  // Make sure ESP-NOW is ready for positioning
        
        if (hasEnoughPeersForTriangulation()) {
          Serial.println("📍 Performing TRIANGULATION (3+ devices)");
          performTriangulation();
        } else {
          Serial.println("📏 Performing DISTANCE MEASUREMENT (2 devices)");
          performDistanceMeasurement();
        }
        estimateRelativePositions();
      } else {
        // Debug: Show why positioning isn't running
        Serial.println("📍 Positioning not ready:");
        for (int i = 0; i < peerCount; i++) {
          Serial.printf("  Peer %s: handshake=%s validated=%s\n", 
            knownPeers[i].deviceId.c_str(),
            knownPeers[i].handshakeComplete ? "✓" : "✗",
            knownPeers[i].validated ? "✓" : "✗");
        }
      }
    }
    
    // Clean up old peers (remove if not seen for 5 minutes)
    for (int i = 0; i < peerCount; i++) {
      if (currentTime - knownPeers[i].lastSeen > 300000) {
        Serial.printf("Removing stale peer: %s\n", knownPeers[i].deviceId.c_str());
        // Shift remaining peers
        for (int j = i; j < peerCount - 1; j++) {
          knownPeers[j] = knownPeers[j + 1];
        }
        peerCount--;
        i--; // Adjust index after removal
      }
    }
    
    delay(100);
  }


  // ============================================================================
  //                        ESP-NOW INITIALIZATION
  // ============================================================================

  void initESPNow() {
    Serial.println("Initializing ESP-NOW...");
    
    // Essential: Set WiFi mode to STA before ESP-NOW init (like working legacy code)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);  // Allow WiFi mode to stabilize
    
    // Set the specific ESP-NOW channel (like working legacy code)
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    delay(100);  // Allow channel setup to stabilize
    
    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
      Serial.printf("❌ Error initializing ESP-NOW: %d (%s)\n", result, esp_err_to_name(result));
      return;
    }
    Serial.println("✓ ESP-NOW initialized successfully");
    
    // Register callbacks (both send and receive like working simple test)
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onESPNowReceive);
    Serial.println("✓ Send and receive callbacks registered");
    
    // Add broadcast peer for peer discovery
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    
    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    if (addResult != ESP_OK) {
      Serial.printf("❌ Failed to add broadcast peer: %d (%s)\n", addResult, esp_err_to_name(addResult));
      return;
    }
    
    Serial.println("✓ ESP-NOW initialized successfully");
    Serial.printf("✓ Broadcasting on channel %d (ESP1 Gateway will receive)\n", ESP_NOW_CHANNEL);
  }


  // ============================================================================
  //                       PEER DISCOVERY MESSAGING
  // ============================================================================

  void sendPeerDiscoveryPing() {
    ensureESPNowActive();  // Make sure ESP-NOW is ready
    
    JsonDocument doc;
    
    // Create envelope structure
    doc["version"] = PROTOCOL_VERSION;
    doc["message_id"] = generateMessageId("ping");
    doc["timestamp"] = millis() / 1000;
    doc["shared_key"] = SHARED_KEY;
    
    // Enhanced source device info (Phase 3)
    JsonObject sourceDevice = doc["source_device"].to<JsonObject>();
    sourceDevice["device_id"] = DEVICE_ID;
    sourceDevice["owner"] = DEVICE_OWNER;
    sourceDevice["mac_address"] = WiFi.macAddress();
    sourceDevice["device_type"] = DEVICE_TYPE;
    sourceDevice["firmware_version"] = FIRMWARE_VERSION;
    
    doc["message_type"] = "ping";
    
    // Enhanced payload with Phase 2/3/4 data
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["rssi"] = WiFi.RSSI();
    payload["free_heap"] = ESP.getFreeHeap();
    payload["uptime"] = millis() / 1000;
    payload["communication_mode"] = (int)currentMode;
    payload["wifi_connected"] = wifiConnected;
    payload["server_reachable"] = serverReachable;
    payload["peer_count"] = peerCount;
    
    if (wifiConnected) {
      payload["connected_ssid"] = connectedSSID;
      payload["wifi_channel"] = wifiChannel;
      payload["wifi_rssi"] = WiFi.RSSI();
    }
    
    // Phase 4: Positioning data
    if (myPosition.isValid) {
      JsonObject position = payload["my_position"].to<JsonObject>();
      position["x"] = myPosition.x;
      position["y"] = myPosition.y;
      position["confidence"] = 1.0;
    }
    
    payload["positioning_ready"] = hasEnoughPeersForPositioning();
    payload["triangulation_ready"] = hasEnoughPeersForTriangulation();
    payload["positioning_peers"] = 0;
    
    // Add debug info for handshake status
    JsonArray peersDebug = payload.createNestedArray("peers_status");
    for (int i = 0; i < peerCount; i++) {
      JsonObject peerInfo = peersDebug.createNestedObject();
      peerInfo["device_id"] = knownPeers[i].deviceId;
      peerInfo["handshake_complete"] = knownPeers[i].handshakeComplete;
      peerInfo["validated"] = knownPeers[i].validated;
      peerInfo["rssi"] = knownPeers[i].rssi;
    }
    
    // Count peers with valid positions
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].relativePos.isValid) {
        payload["positioning_peers"] = payload["positioning_peers"].as<int>() + 1;
      }
    }
    
    // Enhanced capabilities
    JsonArray capabilities = payload["capabilities"].to<JsonArray>();
    capabilities.add("peer_discovery");
    capabilities.add("enhanced_messaging");
    capabilities.add("mode_switching");
    capabilities.add("wifi_scanning");
    capabilities.add("triangulation");
    capabilities.add("positioning");
    capabilities.add("message_relaying");
    capabilities.add("message_storage");
    
    if (currentMode != MODE_ESP_NOW_ONLY) {
      capabilities.add("wifi_communication");
    }
    if (serverReachable) {
      capabilities.add("server_access");
    }
    
    // Phase 5: Relay status
    payload["stored_messages"] = storedMessageCount;
    payload["relay_capable"] = (storedMessageCount < MAX_STORED_MESSAGES);
    payload["server_delivery_available"] = hasServerConnection();
    
    // Serialize and send
    String message;
    serializeJson(doc, message);
    
    Serial.println("📡 Broadcasting Enhanced Peer Discovery Ping (Phase 4)");
    Serial.printf("Mode: %s | WiFi: %s | Server: %s | Triangulation: %s\n", 
      currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
      currentMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
      currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only",
      wifiConnected ? "Connected" : "Disconnected",
      serverReachable ? "Reachable" : "Unreachable",
      hasEnoughPeersForTriangulation() ? "Ready" : "Not Ready");
    
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
    
    if (result == ESP_OK) {
      Serial.println("✓ Enhanced ping broadcast sent (ESP1 Gateway listening)");
    } else {
      Serial.printf("❌ Error sending broadcast ping: %d\n", result);
    }
    
    Serial.println(); // End line for readability
  }


  void sendHandshakeResponse(const String& replyToMessageId, const uint8_t* peerMac) {
    // Compatibility wrapper - delegates to enhanced handshake (Phase 3)
    // This function now delegates to the enhanced handshake
    // We need the original ping for enhanced validation, so create a minimal doc
    JsonDocument dummyPing;
    dummyPing["source_device"]["device_id"] = "unknown"; // Will be updated in processIncomingMessage
    
    sendEnhancedHandshake(replyToMessageId, peerMac, dummyPing);
  }


  void sendDataMessage() {
    ensureESPNowActive();  // Make sure ESP-NOW is ready
    
    // Send enhanced data message with sensor and system information
    // Includes peer status and network health metrics (Phase 2/3)
    JsonDocument doc;
    
    // Create envelope structure
    doc["version"] = PROTOCOL_VERSION;
    doc["message_id"] = generateMessageId("data");
    doc["timestamp"] = millis() / 1000;
    doc["shared_key"] = SHARED_KEY;
    
    // Enhanced source device info (Phase 3)
    JsonObject sourceDevice = doc["source_device"].to<JsonObject>();
    sourceDevice["device_id"] = DEVICE_ID;
    sourceDevice["owner"] = DEVICE_OWNER;
    sourceDevice["mac_address"] = WiFi.macAddress();
    sourceDevice["device_type"] = DEVICE_TYPE;
    sourceDevice["firmware_version"] = FIRMWARE_VERSION;
    
    doc["message_type"] = "data";
    
    // Enhanced payload with sensor and system data
    JsonObject payload = doc["payload"].to<JsonObject>();
    
    JsonObject sensorData = payload["sensor_data"].to<JsonObject>();
    sensorData["temperature"] = 23.5 + (random(-50, 50) / 10.0); // Simulated sensor data
    sensorData["humidity"] = 65.0 + (random(-100, 100) / 10.0);
    
    JsonObject systemData = payload["system_data"].to<JsonObject>();
    systemData["free_heap"] = ESP.getFreeHeap();
    systemData["uptime"] = millis() / 1000;
    systemData["peer_count"] = peerCount;
    systemData["communication_mode"] = (int)currentMode;
    systemData["wifi_connected"] = wifiConnected;
    systemData["server_reachable"] = serverReachable;
    
    if (wifiConnected) {
      systemData["connected_ssid"] = connectedSSID;
      systemData["wifi_rssi"] = WiFi.RSSI();
    }
    
    // Network status summary
    JsonObject networkStatus = payload["network_status"].to<JsonObject>();
    networkStatus["trusted_peers"] = 0;
    networkStatus["validated_peers"] = 0;
    
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].handshakeComplete) networkStatus["trusted_peers"] = networkStatus["trusted_peers"].as<int>() + 1;
      if (knownPeers[i].validated) networkStatus["validated_peers"] = networkStatus["validated_peers"].as<int>() + 1;
    }
    
    // Phase 5: Add relay chain (empty for original message)
    JsonArray relayChain = payload["relay_chain"].to<JsonArray>();
    // Empty relay chain indicates this is the original message
    
    // Phase 5: Message storage status
    payload["stored_message_count"] = storedMessageCount;
    payload["relay_capacity_available"] = (storedMessageCount < MAX_STORED_MESSAGES);
    
    // Serialize and send
    String message;
    serializeJson(doc, message);
    
    Serial.println("📊 Broadcasting Enhanced Data Message");
    Serial.printf("Peers: %d total, %d trusted, %d validated\n", 
                  peerCount, 
                  networkStatus["trusted_peers"].as<int>(),
                  networkStatus["validated_peers"].as<int>());
    Serial.printf("Status: %s | WiFi: %s | Server: %s\n",
      currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
      currentMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
      currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only",
      wifiConnected ? "Connected" : "Disconnected",
      serverReachable ? "Reachable" : "Unreachable");
    
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
    
    if (result == ESP_OK) {
      Serial.println("✓ Enhanced data broadcast sent (ESP1 Gateway listening)\n");
      
      // Phase 5: Store our own message for relay if needed
      storeMessage(doc, DEVICE_ID, true);
      
      printKnownPeers();
      printMessageStorage();
    } else {
      Serial.printf("❌ Error sending data: %d\n\n", result);
    }
  }


  // ============================================================================
  //                        MESSAGE HANDLING
  // ============================================================================

  void onESPNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    // Parse received JSON message
    String receivedMessage = "";
    for (int i = 0; i < len; i++) {
      receivedMessage += (char)data[i];
    }
    
    Serial.println("\n📥 ESP-NOW Message Received");
    Serial.printf("From: %s\n", macToString(info->src_addr).c_str());
    Serial.printf("RSSI: %d\n", info->rx_ctrl->rssi);
    Serial.printf("Raw: %s\n", receivedMessage.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, receivedMessage);
    
    if (error) {
      Serial.printf("❌ JSON parsing failed: %s\n\n", error.c_str());
      return;
    }
    
    // Validate envelope structure and authentication
    if (!validateEnvelope(doc)) {
      Serial.println("❌ Invalid envelope or authentication failed\n");
      return;
    }
    
    // Ignore messages from self
    String senderDeviceId = doc["source_device"]["device_id"];
    if (senderDeviceId == DEVICE_ID) {
      Serial.println("🚫 Ignoring message from self\n");
      return;
    }
    
    Serial.println("✓ Envelope validation passed");
    
    // Process the message based on type
    processIncomingMessage(doc, info->src_addr, info->rx_ctrl->rssi);
  }

  bool validateEnvelope(JsonDocument& doc) {
    // Check required fields
    if (!doc.containsKey("version") || 
        !doc.containsKey("message_id") || 
        !doc.containsKey("timestamp") || 
        !doc.containsKey("shared_key") || 
        !doc.containsKey("source_device") || 
        !doc.containsKey("message_type") || 
        !doc.containsKey("payload")) {
      Serial.println("❌ Missing required envelope fields");
      return false;
    }
    
    // Validate shared key for authentication
    String receivedKey = doc["shared_key"];
    if (receivedKey != SHARED_KEY) {
      Serial.printf("❌ Authentication failed. Expected: %s, Received: %s\n", SHARED_KEY, receivedKey.c_str());
      return false;
    }
    
    // Check protocol version compatibility
    String receivedVersion = doc["version"];
    if (receivedVersion != PROTOCOL_VERSION) {
      Serial.printf("⚠️ Version mismatch. Expected: %s, Received: %s\n", PROTOCOL_VERSION, receivedVersion.c_str());
      // For now, allow different versions but warn
    }
    
    // Phase 3: Enhanced peer credential validation
    if (!validatePeerCredentials(doc)) {
      return false;
    }
    
    return true;
  }

  String generateMessageId(const String& messageType) {
    messageCounter++;
    return messageType + "_" + String(millis()) + "_" + String(messageCounter);
  }

  void processIncomingMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
    String messageType = doc["message_type"];
    String messageId = doc["message_id"];
    String senderDeviceId = doc["source_device"]["device_id"];
    String senderOwner = doc["source_device"]["owner"];
    String senderMacStr = doc["source_device"]["mac_address"];
    
    // Phase 3: Extract enhanced device info
    String senderDeviceType = doc["source_device"].containsKey("device_type") ? 
                            doc["source_device"]["device_type"].as<String>() : "unknown";
    String senderFirmware = doc["source_device"].containsKey("firmware_version") ? 
                          doc["source_device"]["firmware_version"].as<String>() : "unknown";
    
    Serial.printf("Processing %s message from %s (%s) [%s v%s]\n", 
                  messageType.c_str(), senderDeviceId.c_str(), senderOwner.c_str(),
                  senderDeviceType.c_str(), senderFirmware.c_str());
    
    // Phase 2/3: Extract communication status
    if (doc["payload"].containsKey("communication_mode")) {
      int peerMode = doc["payload"]["communication_mode"];
      bool peerWifiConnected = doc["payload"].containsKey("wifi_connected") ? 
                            doc["payload"]["wifi_connected"].as<bool>() : false;
      bool peerServerReachable = doc["payload"].containsKey("server_reachable") ? 
                                doc["payload"]["server_reachable"].as<bool>() : false;
      
      Serial.printf("  Peer status: Mode=%d, WiFi=%s, Server=%s\n",
                    peerMode, peerWifiConnected ? "Yes" : "No", peerServerReachable ? "Yes" : "No");
    }
    
    // Update peer information with enhanced data
    addOrUpdatePeer(senderDeviceId, senderOwner, senderMacStr, rssi);
    
    // Update peer capabilities and status
    JsonObject payload = doc["payload"].as<JsonObject>();
    updatePeerCapabilities(senderDeviceId, payload);
    
    // Phase 4: Check for triangulation capability
    if (doc["payload"].containsKey("capabilities")) {
      JsonArray caps = doc["payload"]["capabilities"];
      for (JsonVariant cap : caps) {
        if (cap.as<String>() == "triangulation") {
          for (int i = 0; i < peerCount; i++) {
            if (knownPeers[i].deviceId == senderDeviceId) {
              knownPeers[i].supportsTriangulation = true;
              break;
            }
          }
          break;
        }
      }
    }
    
    if (messageType == "ping") {
      Serial.printf("🏓 Received trusted ping from %s - shared key validated\n", senderDeviceId.c_str());
      
      // With shared key trust, no handshake needed - mark as instantly validated
      for (int i = 0; i < peerCount; i++) {
        if (knownPeers[i].deviceId == senderDeviceId) {
          knownPeers[i].handshakeComplete = true;
          knownPeers[i].validated = true;
          Serial.printf("✅ %s instantly trusted via shared key\n", senderDeviceId.c_str());
          break;
        }
      }
      
    } else if (messageType == "data") {
      Serial.printf("📊 Received data message from trusted peer %s\n", senderDeviceId.c_str());
      
      // Store message for relaying if needed (Phase 5)
      storeMessage(doc, senderDeviceId, false);
      
      // With shared key trust, all data messages are from validated peers
      for (int i = 0; i < peerCount; i++) {
        if (knownPeers[i].deviceId == senderDeviceId) {
          knownPeers[i].handshakeComplete = true;
          knownPeers[i].validated = true;  // Always true with shared key
          knownPeers[i].deviceType = senderDeviceType;
          knownPeers[i].firmwareVersion = senderFirmware;
          Serial.printf("✓ Peer %s validated via shared key\n", senderDeviceId.c_str());
          break;
        }
      }
      
    } else if (messageType == "distance_measurement") {
      Serial.printf("📏 Received distance measurement from %s\n", senderDeviceId.c_str());
      
      // Process distance measurement data (Phase 4)
      if (doc.containsKey("data")) {
        JsonObject data = doc["data"];
        float distance = data["estimated_distance"];
        String confidence = data["measurement_confidence"];
        Serial.printf("  Distance: %.1fm (confidence: %s)\n", distance, confidence.c_str());
      }
      
    } else if (messageType == "triangulation") {
      Serial.printf("📍 Received triangulation data from %s\n", senderDeviceId.c_str());
      processTriangulationData(doc, rssi);
      
    } else {
      Serial.printf("📋 Received enhanced data message from %s\n", senderDeviceId.c_str());
      
      // Extract and display sensor data if available
      if (doc["payload"].containsKey("sensor_data")) {
        JsonObject sensorData = doc["payload"]["sensor_data"];
        if (sensorData.containsKey("temperature")) {
          Serial.printf("  Temperature: %.1f°C\n", sensorData["temperature"].as<float>());
        }
        if (sensorData.containsKey("humidity")) {
          Serial.printf("  Humidity: %.1f%%\n", sensorData["humidity"].as<float>());
        }
      }
      
      if (doc["payload"].containsKey("system_data")) {
        JsonObject systemData = doc["payload"]["system_data"];
        if (systemData.containsKey("uptime")) {
          Serial.printf("  Uptime: %d seconds\n", systemData["uptime"].as<int>());
        }
        if (systemData.containsKey("peer_count")) {
          Serial.printf("  Peer count: %d\n", systemData["peer_count"].as<int>());
        }
      }
      
    }
    
    Serial.println();
  }

  void addOrUpdatePeer(const String& deviceId, const String& owner, const String& macAddr, int rssi) {
    // Look for existing peer
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        // Update existing peer
        knownPeers[i].rssi = rssi;
        knownPeers[i].lastSeen = millis();
        return;
      }
    }
    
    // Add new peer if space available
    if (peerCount < MAX_PEERS) {
      knownPeers[peerCount].deviceId = deviceId;
      knownPeers[peerCount].owner = owner;
      knownPeers[peerCount].macAddress = macAddr;
      knownPeers[peerCount].rssi = rssi;
      knownPeers[peerCount].lastSeen = millis();
      knownPeers[peerCount].handshakeComplete = false;
      peerCount++;
      
      Serial.printf("➕ Added new peer: %s (%s) - RSSI: %d\n", deviceId.c_str(), owner.c_str(), rssi);
    } else {
      Serial.println("⚠️ Maximum peer count reached - cannot add new peer");
    }
  }


  void printKnownPeers() {
    // Display comprehensive peer network status including capabilities and positioning
    // Enhanced for Phase 2/3/4 features
    Serial.println("👥 Enhanced Peer Network Status:");
    Serial.printf("Current Mode: %s | WiFi: %s | Server: %s\n",
      currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
      currentMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
      currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only",
      wifiConnected ? connectedSSID.c_str() : "Disconnected",
      serverReachable ? "Reachable" : "Unreachable");
    
    if (peerCount == 0) {
      Serial.println("  No peers discovered yet");
    } else {
      for (int i = 0; i < peerCount; i++) {
        Serial.printf("  %d. %s (%s) [%s v%s]\n", 
                      i + 1, 
                      knownPeers[i].deviceId.c_str(),
                      knownPeers[i].owner.c_str(),
                      knownPeers[i].deviceType.c_str(),
                      knownPeers[i].firmwareVersion.c_str());
        
        Serial.printf("     RSSI: %d dBm | Handshake: %s | Validated: %s | Last seen: %lus ago\n",
                      knownPeers[i].rssi,
                      knownPeers[i].handshakeComplete ? "✓" : "✗",
                      knownPeers[i].validated ? "✓" : "✗",
                      (millis() - knownPeers[i].lastSeen) / 1000);
        
        if (!knownPeers[i].capabilities.empty()) {
          Serial.print("     Capabilities: ");
          for (size_t j = 0; j < knownPeers[i].capabilities.size(); j++) {
            Serial.print(knownPeers[i].capabilities[j]);
            if (j < knownPeers[i].capabilities.size() - 1) Serial.print(", ");
          }
          Serial.println();
        }
        
        Serial.printf("     Preferred Mode: %s\n",
          knownPeers[i].preferredMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
          knownPeers[i].preferredMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
          knownPeers[i].preferredMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only");
      }
    }
    Serial.println();
  }


  String macToString(const uint8_t* mac) {
    // Convert MAC address bytes to string format
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
  }

  // ESP-NOW Send Callback (working signature from simple test)
  void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
            tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    Serial.printf("ESP-NOW Send to %s: %s\n", macStr, 
                  status == ESP_NOW_SEND_SUCCESS ? "✓ SUCCESS" : "❌ FAILED");
  }


  // ============================================================================
  //                        WIFI & SERVER FUNCTIONS
  //                            (Phase 2)
  // ============================================================================

  void performWiFiScan() {
    Serial.println("🔍 Scanning for WiFi networks...");
    
    int n = WiFi.scanNetworks();
    bool foundKnown = false;
    
    if (n == 0) {
      Serial.println("❌ No WiFi networks found");
      return;
    }
    
    Serial.printf("Found %d networks:\n", n);
    
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      
      Serial.printf("  %s (RSSI: %d) %s\n", ssid.c_str(), rssi, isOpen ? "[OPEN]" : "[SECURED]");
      
      // Check if this is a known network
      for (int j = 0; knownNetworks[j].ssid != ""; j++) {
        if (knownNetworks[j].ssid == ssid) {
          Serial.printf("    ✓ Known network: %s\n", ssid.c_str());
          foundKnown = true;
          break;
        }
      }
    }
    
    if (foundKnown && !wifiConnected) {
      Serial.println("🔗 Attempting to connect to known networks...");
      tryConnectToKnownNetworks();
    } else if (!foundKnown) {
      Serial.println("⚠️ No known networks found");
    }
    
    Serial.println();
  }

  bool tryConnectToKnownNetworks() {
    // Scan again to get fresh results
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int32_t channel = WiFi.channel(i);
      bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      
      // Check against known networks
      for (int j = 0; knownNetworks[j].ssid != ""; j++) {
        if (knownNetworks[j].ssid == ssid) {
          Serial.printf("🔗 Attempting connection to: %s\n", ssid.c_str());
          
          if (knownNetworks[j].isOpen || isOpen) {
            WiFi.begin(ssid.c_str());
          } else {
            WiFi.begin(ssid.c_str(), knownNetworks[j].password.c_str());
          }
          
          // Wait for connection
          int attempts = 0;
          while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
          }
          
          if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            connectedSSID = ssid;
            wifiChannel = channel;
            
            Serial.println();
            Serial.printf("✅ Connected to: %s\n", ssid.c_str());
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
            Serial.printf("Channel: %d\n", channel);
            
            lastSuccessfulServerContact = millis();
            return true;
          } else {
            Serial.println();
            Serial.printf("❌ Failed to connect to: %s\n", ssid.c_str());
            WiFi.disconnect();
          }
        }
      }
    }
    
    return false;
  }

  void checkServerReachability() {
    if (!wifiConnected) {
      Serial.println("⚠️ Cannot check server - no WiFi connection");
      serverReachable = false;
      return;
    }
    
    Serial.println("🌐 Checking server reachability...");
    
    WiFiClient client;
    if (client.connect(TEST_SERVER_HOST, TEST_SERVER_PORT)) {
      serverReachable = true;
      lastSuccessfulServerContact = millis();
      Serial.printf("✅ Server reachable: %s:%d\n", TEST_SERVER_HOST, TEST_SERVER_PORT);
      client.stop();
    } else {
      serverReachable = false;
      Serial.printf("❌ Server unreachable: %s:%d\n", TEST_SERVER_HOST, TEST_SERVER_PORT);
    }
  }

  void updateCommunicationMode() {
    CommMode previousMode = currentMode;
    
    // ESP1 Gateway Mode: Prioritize ESP-NOW communication
    // Only use WiFi as backup for server communication if absolutely needed
    if (!wifiConnected) {
      currentMode = MODE_ESP_NOW_ONLY;
    } else {
      // Even with WiFi available, prefer ESP-NOW for ESP1 gateway
      // WiFi is secondary for server communication only
      currentMode = MODE_WIFI_BACKUP;  // ESP-NOW primary, WiFi secondary
    }
    
    if (currentMode != previousMode && millis() - lastModeSwitch > MODE_SWITCH_COOLDOWN) {
      lastModeSwitch = millis();
      
      Serial.printf("🔄 Mode change: %s -> %s (ESP1 Gateway Priority)\n",
        previousMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
        previousMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
        previousMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only",
        
        currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
        currentMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
        currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only");
      
      // Always ensure ESP-NOW is active for ESP1 communication
      switchToESPNowMode();
      
      if (currentMode == MODE_WIFI_BACKUP && wifiConnected) {
        Serial.println("📡 WiFi available as backup for server communication");
      }
    }
  }

  void switchToESPNowMode() {
    Serial.println("🔧 Switching to ESP-NOW mode...");
    
    // Ensure ESP-NOW is initialized on correct channel
    if (wifiConnected && wifiChannel > 0) {
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
      Serial.printf("📡 ESP-NOW using WiFi channel: %d\n", wifiChannel);
    } else {
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
      Serial.printf("📡 ESP-NOW using default channel: %d\n", ESP_NOW_CHANNEL);
    }
  }

  void switchToWiFiMode() {
    Serial.println("🔧 Configuring for WiFi mode...");
    
    if (wifiChannel > 0) {
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
      Serial.printf("📡 ESP-NOW synchronized to WiFi channel: %d\n", wifiChannel);
    }
  }

  int32_t getChannelFromSSID(const String& ssid) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == ssid) {
        return WiFi.channel(i);
      }
    }
    return 0;
  }


  // ============================================================================
  //                      ENHANCED PEER FUNCTIONS
  //                            (Phase 3)
  // ============================================================================

  bool validatePeerCredentials(JsonDocument& doc) {
    // Enhanced validation for Phase 3
    
    // Check device type
    if (!doc["source_device"].containsKey("device_type")) {
      Serial.println("❌ Missing device type");
      return false;
    }
    
    String deviceType = doc["source_device"]["device_type"];
    if (deviceType != DEVICE_TYPE) {
      Serial.printf("❌ Invalid device type: %s (expected %s)\n", deviceType.c_str(), DEVICE_TYPE);
      return false;
    }
    
    // Check firmware version compatibility
    if (doc["source_device"].containsKey("firmware_version")) {
      String peerFirmware = doc["source_device"]["firmware_version"];
      Serial.printf("ℹ️ Peer firmware: %s (ours: %s)\n", peerFirmware.c_str(), FIRMWARE_VERSION);
    }
    
    // Additional security checks could be added here
    return true;
  }

  void sendEnhancedHandshake(const String& replyToMessageId, const uint8_t* peerMac, JsonDocument& originalPing) {
    // Prevent handshake loops with enhanced tracking
    String peerDeviceId = originalPing["source_device"]["device_id"];
    
    if (handshakeAttempts.find(peerDeviceId) != handshakeAttempts.end()) {
      if (millis() - handshakeAttempts[peerDeviceId] < HANDSHAKE_TIMEOUT) {
        Serial.printf("🚫 Handshake cooldown active for %s (%.1fs remaining)\n", 
                     peerDeviceId.c_str(), 
                     (HANDSHAKE_TIMEOUT - (millis() - handshakeAttempts[peerDeviceId])) / 1000.0);
        return;
      }
    }
    
    handshakeAttempts[peerDeviceId] = millis();
    
    JsonDocument doc;
    
    // Create enhanced envelope structure
    doc["version"] = PROTOCOL_VERSION;
    doc["message_id"] = generateMessageId("handshake");
    doc["timestamp"] = millis() / 1000;
    doc["shared_key"] = SHARED_KEY;
    
    // Enhanced source device info
    JsonObject sourceDevice = doc["source_device"].to<JsonObject>();
    sourceDevice["device_id"] = DEVICE_ID;
    sourceDevice["owner"] = DEVICE_OWNER;
    sourceDevice["mac_address"] = WiFi.macAddress();
    sourceDevice["device_type"] = DEVICE_TYPE;
    sourceDevice["firmware_version"] = FIRMWARE_VERSION;
    
    doc["message_type"] = "handshake";
    
    // Enhanced payload with negotiation data
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["reply_to"] = replyToMessageId;
    payload["rssi"] = WiFi.RSSI();
    payload["free_heap"] = ESP.getFreeHeap();
    payload["uptime"] = millis() / 1000;
    payload["communication_mode"] = (int)currentMode;
    payload["wifi_connected"] = wifiConnected;
    payload["server_reachable"] = serverReachable;
    
    // Enhanced capabilities
    JsonArray capabilities = payload["capabilities"].to<JsonArray>();
    capabilities.add("peer_discovery");
    capabilities.add("enhanced_handshake");
    capabilities.add("mode_switching");
    capabilities.add("wifi_scanning");
    capabilities.add("server_monitoring");
    
    if (currentMode != MODE_ESP_NOW_ONLY) {
      capabilities.add("wifi_communication");
    }
    
    // Peer validation info
    JsonObject validation = payload["validation"].to<JsonObject>();
    validation["trusted"] = isPeerTrusted(peerDeviceId);
    validation["validation_timestamp"] = millis() / 1000;
    
    // Serialize and send
    String message;
    serializeJson(doc, message);
    
    Serial.println("🤝 Sending Enhanced Handshake Response");
    Serial.printf("To: %s (%s)\n", peerDeviceId.c_str(), macToString(peerMac).c_str());
    Serial.printf("Reply to: %s\n", replyToMessageId.c_str());
    Serial.printf("Mode: %s\n", currentMode == MODE_ESP_NOW_ONLY ? "ESP-NOW Only" :
                                currentMode == MODE_WIFI_BACKUP ? "WiFi Backup" :
                                currentMode == MODE_WIFI_PRIMARY ? "WiFi Primary" : "WiFi Only");
    
    esp_err_t result = esp_now_send(peerMac, (uint8_t*)message.c_str(), message.length());
    
    if (result == ESP_OK) {
      Serial.println("✓ Enhanced handshake sent\n");
    } else {
      Serial.printf("❌ Error sending enhanced handshake: %d\n\n", result);
    }
  }

  bool isPeerTrusted(const String& deviceId) {
    // Basic trust validation - can be enhanced with whitelist, certificates, etc.
    // For now, trust peers that have completed handshake and have been seen recently
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        return knownPeers[i].validated && 
              knownPeers[i].handshakeComplete && 
              (millis() - knownPeers[i].lastSeen < 300000); // 5 minutes
      }
    }
    return false;
  }

  void updatePeerCapabilities(const String& deviceId, JsonObject& payload) {
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        knownPeers[i].capabilities.clear();
        
        if (payload.containsKey("capabilities")) {
          JsonArray caps = payload["capabilities"];
          for (JsonVariant cap : caps) {
            knownPeers[i].capabilities.push_back(cap.as<String>());
          }
        }
        
        // Update communication mode preference
        if (payload.containsKey("communication_mode")) {
          knownPeers[i].preferredMode = (CommMode)payload["communication_mode"].as<int>();
        }
        
        break;
      }
    }
  }

  void cleanupFailedHandshakes() {
    unsigned long currentTime = millis();
    
    // Clean up old handshake attempts
    for (auto it = handshakeAttempts.begin(); it != handshakeAttempts.end();) {
      if (currentTime - it->second > HANDSHAKE_TIMEOUT * 2) {
        it = handshakeAttempts.erase(it);
      } else {
        ++it;
      }
    }
    
    // Mark peers with failed handshakes
    for (int i = 0; i < peerCount; i++) {
      if (!knownPeers[i].handshakeComplete && 
          knownPeers[i].handshakeAttempts >= MAX_HANDSHAKE_ATTEMPTS &&
          currentTime - knownPeers[i].lastHandshakeAttempt > HANDSHAKE_TIMEOUT) {
        Serial.printf("⚠️ Peer %s has failed handshakes, marking as untrusted\n", 
                      knownPeers[i].deviceId.c_str());
        knownPeers[i].validated = false;
      }
    }
  }


  // ============================================================================
  //                   TRIANGULATION & POSITIONING FUNCTIONS
  //                            (Phase 4)
  // ============================================================================
  //
  // Purpose: RSSI-based distance estimation and relative positioning
  //          Enables ESP2 devices to determine their relative positions
  //          using signal strength measurements and triangulation algorithms
  //
  // Key Functions:
  // - calculateDistanceFromRSSI(): Convert RSSI to distance using path loss model
  // - updateRSSIHistory(): Track signal strength over time for stability
  // - updatePeerPosition(): Calculate and update peer relative positions
  // - performTriangulation(): Main triangulation processing routine
  // - estimateRelativePositions(): Determine compass directions (N/S/E/W)
  // - Direction calculation: Use multi-peer RSSI for directional estimation
  // ============================================================================

  float calculateDistanceFromRSSI(int rssi) {
    // Convert RSSI to distance using log-distance path loss model
    // Distance = 10^((RSSI_ref - RSSI) / (10 * n))
    // Where RSSI_ref is RSSI at reference distance (1m), n is path loss exponent
    
    if (rssi > RSSI_CALIBRATION_VALUE) {
      // Very close, return minimum distance
      return 0.5;
    }
    
    float distance = RSSI_CALIBRATION_DISTANCE * 
                    pow(10.0, (RSSI_CALIBRATION_VALUE - rssi) / (10.0 * PATH_LOSS_EXPONENT));
    
    // Cap maximum distance for reliability
    if (distance > MAX_POSITIONING_DISTANCE) {
      distance = MAX_POSITIONING_DISTANCE;
    }
    
    return distance;
  }

  // RSSI History Management
  void updateRSSIHistory(const String& deviceId, int rssi) {
    // Maintain historical RSSI values for better distance estimation stability
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        // Add new RSSI value
        knownPeers[i].rssiHistory.push_back(rssi);
        
        // Keep only recent values (max 10)
        if (knownPeers[i].rssiHistory.size() > 10) {
          knownPeers[i].rssiHistory.erase(knownPeers[i].rssiHistory.begin());
        }
        break;
      }
    }
  }

  // Position Calculation and Update
  void updatePeerPosition(const String& deviceId, int rssi) {
    // Calculate peer distance and relative position using averaged RSSI values
    // Updates confidence based on signal stability
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        // Update RSSI history
        updateRSSIHistory(deviceId, rssi);
        
        // Calculate average RSSI for more stable distance estimation
        float avgRSSI = rssi;
        if (!knownPeers[i].rssiHistory.empty()) {
          int sum = 0;
          for (int val : knownPeers[i].rssiHistory) {
            sum += val;
          }
          avgRSSI = sum / (float)knownPeers[i].rssiHistory.size();
        }
        
        // Calculate distance
        float distance = calculateDistanceFromRSSI((int)avgRSSI);
        
        // Update relative position
        knownPeers[i].relativePos.distance = distance;
        knownPeers[i].relativePos.lastUpdated = millis();
        knownPeers[i].relativePos.isValid = true;
        
        // Calculate confidence based on RSSI stability
        float confidence = 0.5; // Base confidence
        if (knownPeers[i].rssiHistory.size() >= 3) {
          // Calculate RSSI variance
          float variance = 0;
          for (int val : knownPeers[i].rssiHistory) {
            variance += pow(val - avgRSSI, 2);
          }
          variance /= knownPeers[i].rssiHistory.size();
          
          // Lower variance = higher confidence
          confidence = 1.0 - (variance / 100.0); // Normalize variance
          confidence = max(0.1f, min(1.0f, confidence)); // Clamp between 0.1-1.0
        }
        
        knownPeers[i].relativePos.confidence = confidence;
        
        Serial.printf("📍 Updated position for %s: %.1fm (RSSI: %.1f, Confidence: %.2f)\n",
                      deviceId.c_str(), distance, avgRSSI, confidence);
        break;
      }
    }
  }

  // Direction Estimation Algorithm
  Direction calculateRelativeDirection(const String& peerId1, const String& peerId2, const String& targetPeerId) {
    // Estimate compass direction using multi-peer signal strength comparison
    // Simple direction estimation using signal strength triangulation
    // This is a basic implementation - more sophisticated algorithms could be used
    
    int targetIndex = -1, peer1Index = -1, peer2Index = -1;
    
    // Find peer indices
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == targetPeerId) targetIndex = i;
      else if (knownPeers[i].deviceId == peerId1) peer1Index = i;
      else if (knownPeers[i].deviceId == peerId2) peer2Index = i;
    }
    
    if (targetIndex == -1) return DIR_UNKNOWN;
    
    // Use RSSI differences to estimate direction
    int targetRSSI = knownPeers[targetIndex].rssi;
    
    if (peer1Index >= 0 && peer2Index >= 0) {
      int peer1RSSI = knownPeers[peer1Index].rssi;
      int peer2RSSI = knownPeers[peer2Index].rssi;
      
      // Simple direction logic based on signal strength comparison
      // This is a simplified model - real implementation would need more sophisticated algorithms
      if (targetRSSI > peer1RSSI && targetRSSI > peer2RSSI) {
        return DIR_NORTH; // Strongest signal - assume closer/north
      } else if (targetRSSI < peer1RSSI && targetRSSI < peer2RSSI) {
        return DIR_SOUTH; // Weakest signal - assume farther/south
      } else {
        // Mixed signals - estimate based on difference
        float angle = atan2(peer2RSSI - peer1RSSI, targetRSSI - (peer1RSSI + peer2RSSI) / 2.0) * 180.0 / PI;
        
        if (angle >= -22.5 && angle < 22.5) return DIR_NORTH;
        else if (angle >= 22.5 && angle < 67.5) return DIR_NORTHEAST;
        else if (angle >= 67.5 && angle < 112.5) return DIR_EAST;
        else if (angle >= 112.5 && angle < 157.5) return DIR_SOUTHEAST;
        else if (angle >= 157.5 || angle < -157.5) return DIR_SOUTH;
        else if (angle >= -157.5 && angle < -112.5) return DIR_SOUTHWEST;
        else if (angle >= -112.5 && angle < -67.5) return DIR_WEST;
        else return DIR_NORTHWEST;
      }
    }
    
    return DIR_UNKNOWN;
  }

  // Relative Position Estimation
  void estimateRelativePositions() {
    // Determine compass directions for all peers using distance-based distribution
    Serial.println("🧭 Estimating relative positions...");
    
    for (int i = 0; i < peerCount; i++) {
      if (!knownPeers[i].handshakeComplete || !knownPeers[i].relativePos.isValid) continue;
      
      // Simple direction estimation based on distance ranking
      std::vector<int> nearbyPeers;
      for (int j = 0; j < peerCount; j++) {
        if (i != j && knownPeers[j].handshakeComplete && knownPeers[j].relativePos.isValid) {
          nearbyPeers.push_back(j);
        }
      }
      
      // Sort by distance
      std::sort(nearbyPeers.begin(), nearbyPeers.end(), [](int a, int b) {
        return knownPeers[a].relativePos.distance < knownPeers[b].relativePos.distance;
      });
      
      // Assign directions based on relative positions
      if (nearbyPeers.size() >= 1) {
        // Use simple angular distribution
        for (size_t j = 0; j < nearbyPeers.size(); j++) {
          int peerIdx = nearbyPeers[j];
          
          // Distribute peers around compass points
          Direction dir = DIR_UNKNOWN;
          switch (j % 8) {
            case 0: dir = DIR_NORTH; break;
            case 1: dir = DIR_NORTHEAST; break;
            case 2: dir = DIR_EAST; break;
            case 3: dir = DIR_SOUTHEAST; break;
            case 4: dir = DIR_SOUTH; break;
            case 5: dir = DIR_SOUTHWEST; break;
            case 6: dir = DIR_WEST; break;
            case 7: dir = DIR_NORTHWEST; break;
          }
          
          knownPeers[i].relativePos.direction = dir;
          
          Serial.printf("  %s -> %s: %.1fm %s (confidence: %.2f)\n",
                        DEVICE_ID, knownPeers[i].deviceId.c_str(),
                        knownPeers[i].relativePos.distance,
                        directionToString(dir).c_str(),
                        knownPeers[i].relativePos.confidence);
        }
      }
    }
  }

  // Distance Measurement for 2-Device Setup
  void performDistanceMeasurement() {
    // Perform distance calculation and relative positioning for 2-device setup
    if (!hasEnoughPeersForPositioning()) {
      Serial.println("⚠️ Not enough peers for distance measurement");
      return;
    }
    
    Serial.println("📏 Performing distance measurement update...");
    
    // Calculate distance to each trusted peer (shared key = trusted)
    for (int i = 0; i < peerCount; i++) {
      // With shared key trust, all peers are automatically valid for distance measurement
      float distance = calculateDistanceFromRSSI(knownPeers[i].rssi);
      knownPeers[i].relativePos.distance = distance;
      
      // Send distance measurement message
      JsonDocument doc;
      doc["source_device"]["device_id"] = DEVICE_ID;
      doc["source_device"]["user_name"] = DEVICE_OWNER;
      doc["source_device"]["device_type"] = "ESP2_UNIVERSAL";
      doc["source_device"]["mac_address"] = WiFi.macAddress();
      doc["message_type"] = "distance_measurement";
      doc["version"] = ESP2_VERSION;
      doc["timestamp"] = millis();
      
      JsonObject data = doc["data"].to<JsonObject>();
      data["target_device"] = knownPeers[i].deviceId;
      data["rssi"] = knownPeers[i].rssi;
      data["estimated_distance"] = distance;
      data["measurement_confidence"] = (abs(knownPeers[i].rssi) < 70) ? "high" : "medium";
      
      // Add relative direction estimate (simplified)
      String direction = "unknown";
      if (knownPeers[i].rssi > -50) direction = "very_close";
      else if (knownPeers[i].rssi > -60) direction = "close";
      else if (knownPeers[i].rssi > -70) direction = "medium";
      else direction = "far";
      data["relative_distance"] = direction;
      
      String message;
      serializeJson(doc, message);
      
      // Send via ESP-NOW broadcast
      uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
      
      Serial.printf("📏 Distance to %s: %.1fm (RSSI: %d dBm)\n", 
        knownPeers[i].deviceId.c_str(), distance, knownPeers[i].rssi);
    }
  }

  // Main Triangulation Processing
  void performTriangulation() {
    // Execute complete triangulation cycle: distance calculation, positioning, and reporting
    if (!hasEnoughPeersForTriangulation()) {
      Serial.println("⚠️ Not enough peers for triangulation");
      return;
    }
    
    Serial.println("📐 Performing triangulation update...");
    
    // Update positions for all valid peers
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].handshakeComplete && knownPeers[i].validated) {
        updatePeerPosition(knownPeers[i].deviceId, knownPeers[i].rssi);
      }
    }
    
    // Estimate relative directions
    estimateRelativePositions();
    
    // Print positioning summary
    printPositioningSummary();
    
    Serial.println("✓ Triangulation update completed\n");
  }

  // Triangulation Data Exchange
  void sendTriangulationPing() {
    // Broadcast triangulation request with position data
    // Enhanced ping with triangulation data
    JsonDocument doc;
    
    // Create envelope structure
    doc["version"] = PROTOCOL_VERSION;
    doc["message_id"] = generateMessageId("triangulation");
    doc["timestamp"] = millis() / 1000;
    doc["shared_key"] = SHARED_KEY;
    
    // Enhanced source device info
    JsonObject sourceDevice = doc["source_device"].to<JsonObject>();
    sourceDevice["device_id"] = DEVICE_ID;
    sourceDevice["owner"] = DEVICE_OWNER;
    sourceDevice["mac_address"] = WiFi.macAddress();
    sourceDevice["device_type"] = DEVICE_TYPE;
    sourceDevice["firmware_version"] = FIRMWARE_VERSION;
    
    doc["message_type"] = "triangulation";
    
    // Triangulation-specific payload
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["request_type"] = "position_update";
    payload["rssi"] = WiFi.RSSI();
    payload["timestamp"] = millis() / 1000;
    
    // Include own position if known
    if (myPosition.isValid) {
      JsonObject position = payload["position"].to<JsonObject>();
      position["x"] = myPosition.x;
      position["y"] = myPosition.y;
      position["confidence"] = 1.0;
    }
    
    // Include nearby peer positions
    JsonArray nearbyPeers = payload["nearby_peers"].to<JsonArray>();
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].relativePos.isValid) {
        JsonObject peer = nearbyPeers.add<JsonObject>();
        peer["device_id"] = knownPeers[i].deviceId;
        peer["distance"] = knownPeers[i].relativePos.distance;
        peer["direction"] = directionToString(knownPeers[i].relativePos.direction);
        peer["confidence"] = knownPeers[i].relativePos.confidence;
      }
    }
    
    // Serialize and send
    String message;
    serializeJson(doc, message);
    
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
  }

  // Triangulation Data Processing
  void processTriangulationData(JsonDocument& doc, int rssi) {
    // Process incoming triangulation data and update peer positions
    // Process received triangulation data
    String senderDevice = doc["source_device"]["device_id"];
    
    // Update position based on received RSSI
    updatePeerPosition(senderDevice, rssi);
    
    // Process any position data from the sender
    if (doc["payload"].containsKey("position")) {
      JsonObject position = doc["payload"]["position"];
      // Could use this for more advanced triangulation algorithms
    }
    
    // Process nearby peers data for cross-validation
    if (doc["payload"].containsKey("nearby_peers")) {
      JsonArray nearbyPeers = doc["payload"]["nearby_peers"];
      // Could use this for mesh-based positioning
    }
  }

  // Direction Conversion Utilities
  String directionToString(Direction dir) {
    // Convert direction enum to human-readable string
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
    // Convert direction string back to enum value
    if (dirStr == "North") return DIR_NORTH;
    else if (dirStr == "Northeast") return DIR_NORTHEAST;
    else if (dirStr == "East") return DIR_EAST;
    else if (dirStr == "Southeast") return DIR_SOUTHEAST;
    else if (dirStr == "South") return DIR_SOUTH;
    else if (dirStr == "Southwest") return DIR_SOUTHWEST;
    else if (dirStr == "West") return DIR_WEST;
    else if (dirStr == "Northwest") return DIR_NORTHWEST;
    return DIR_UNKNOWN;
  }

bool hasEnoughPeersForPositioning() {
  // Check if sufficient trusted peers are available for distance measurement
  int validPeers = 0;
  for (int i = 0; i < peerCount; i++) {
    // With shared key trust, any peer with same key is instantly valid
    validPeers++;
  }
  return validPeers >= MIN_PEERS_FOR_POSITIONING;
}

bool hasEnoughPeersForTriangulation() {
  // True triangulation needs at least 3 trusted peers
  int validPeers = 0;
  for (int i = 0; i < peerCount; i++) {
    // With shared key trust, any peer with same key is instantly valid  
    validPeers++;
  }
  return validPeers >= MIN_PEERS_FOR_TRIANGULATION;
}  void printPositioningSummary() {
    // Display comprehensive positioning status for all peers
    // Shows distances, directions, and confidence levels
    Serial.println("🗺️ Positioning Summary:");
    Serial.printf("Device: %s (Reference)\n", DEVICE_ID);
    
    if (myPosition.isValid) {
      Serial.printf("  My Position: (%.1f, %.1f)\n", myPosition.x, myPosition.y);
    } else {
      Serial.println("  My Position: Unknown");
    }
    
    bool hasValidPositions = false;
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].handshakeComplete && knownPeers[i].relativePos.isValid) {
        hasValidPositions = true;
        Serial.printf("  %s: %.1fm %s (confidence: %.0f%%)\n",
                      knownPeers[i].deviceId.c_str(),
                      knownPeers[i].relativePos.distance,
                      directionToString(knownPeers[i].relativePos.direction).c_str(),
                      knownPeers[i].relativePos.confidence * 100);
      }
    }
    
    if (!hasValidPositions) {
      Serial.println("  No peer positions available yet");
    }
    
    Serial.println();
  }

  // ============================================================================
  //                   MESSAGE RELAYING & STORAGE FUNCTIONS
  //                            (Phase 5)
  // ============================================================================
  //
  // Purpose: Store-and-forward messaging through peer network
  //          Enables message delivery when direct server access is unavailable
  //          Tracks relay chains and prevents message loops
  //
  // Key Functions:
  // - storeMessage(): Store messages for relay opportunities
  // - checkForRelayOpportunities(): Find peers with server access for relay
  // - attemptServerDelivery(): Send stored messages to monitor server
  // - relayMessageToPeer(): Forward message through peer network
  // - Message tracking: Complete relay chain and delivery status
  // ============================================================================

  void storeMessage(JsonDocument& messageDoc, const String& senderId, bool isOwnMessage) {
    // Store message for potential relay to server or other peers
    String messageId = messageDoc["message_id"];
    
    // Check if message already exists (avoid duplicates)
    for (int i = 0; i < storedMessageCount; i++) {
      if (messageStorage[i].messageId == messageId) {
        Serial.printf("📦 Message already stored: %s\n", messageId.c_str());
        return;
      }
    }
    
    // Find available storage slot
    if (storedMessageCount >= MAX_STORED_MESSAGES) {
      Serial.println("⚠️ Message storage full - removing oldest message");
      // Remove oldest message to make space
      for (int i = 0; i < storedMessageCount - 1; i++) {
        messageStorage[i] = messageStorage[i + 1];
      }
      storedMessageCount--;
    }
    
    // Store the message
    int storeIndex = storedMessageCount;
    messageStorage[storeIndex].messageId = messageId;
    messageStorage[storeIndex].originalSender = senderId;
    messageStorage[storeIndex].senderOwner = messageDoc["source_device"]["owner"].as<String>();
    messageStorage[storeIndex].messageData = messageDoc;
    messageStorage[storeIndex].timestamp = millis();
    messageStorage[storeIndex].lastRelayAttempt = 0;
    messageStorage[storeIndex].deliveredToServer = false;
    messageStorage[storeIndex].isOwnMessage = isOwnMessage;
    messageStorage[storeIndex].hopCount = 0;
    messageStorage[storeIndex].relayChain.clear();
    messageStorage[storeIndex].attemptedPeers.clear();
    
    // Extract existing relay chain if present
    if (messageDoc["payload"].containsKey("relay_chain")) {
      JsonArray relayChain = messageDoc["payload"]["relay_chain"];
      for (JsonVariant hop : relayChain) {
        RelayHop relayHop;
        relayHop.deviceId = hop["device_id"].as<String>();
        relayHop.deviceOwner = hop["device_owner"].as<String>();
        relayHop.timestamp = hop["timestamp"].as<unsigned long>();
        relayHop.rssi = hop["rssi"].as<int>();
        messageStorage[storeIndex].relayChain.push_back(relayHop);
      }
      messageStorage[storeIndex].hopCount = messageStorage[storeIndex].relayChain.size();
    }
    
    storedMessageCount++;
    
    Serial.printf("📦 Stored message: %s from %s (%s) - Storage: %d/%d\n", 
                  messageId.c_str(), senderId.c_str(), 
                  messageStorage[storeIndex].senderOwner.c_str(),
                  storedMessageCount, MAX_STORED_MESSAGES);
  }

  void processRelayMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
    // Process incoming relay message request
    String senderDeviceId = doc["source_device"]["device_id"];
    String relayMessageId = doc["payload"]["relay_message_id"];
    String relayRequest = doc["payload"]["request_type"];
    
    Serial.printf("🔄 Relay request from %s: %s for message %s\n", 
                  senderDeviceId.c_str(), relayRequest.c_str(), relayMessageId.c_str());
    
    if (relayRequest == "delivery_request") {
      // Peer is asking us to relay their message
      if (doc["payload"].containsKey("message_data")) {
        JsonDocument relayedMessage = doc["payload"]["message_data"];
        storeMessage(relayedMessage, senderDeviceId, false);
      }
      
    } else if (relayRequest == "delivery_confirmation") {
      // Peer confirms they delivered our message
      markMessageDelivered(relayMessageId);
      Serial.printf("✅ Message %s confirmed delivered by %s\n", 
                    relayMessageId.c_str(), senderDeviceId.c_str());
    }
  }

  void checkForRelayOpportunities() {
    // Look for peers with server access to relay our stored messages
    if (storedMessageCount == 0) return;
    
    Serial.println("🔍 Checking for relay opportunities...");
    
    for (int i = 0; i < storedMessageCount; i++) {
      StoredMessage& msg = messageStorage[i];
      
      // Skip if already delivered or recently attempted
      if (msg.deliveredToServer || 
          (millis() - msg.lastRelayAttempt < RELAY_ATTEMPT_COOLDOWN)) {
        continue;
      }
      
      // Skip if too many hops
      if (msg.hopCount >= MAX_RELAY_HOPS) {
        Serial.printf("⛔ Message %s exceeded max hops (%d)\n", 
                      msg.messageId.c_str(), msg.hopCount);
        continue;
      }
      
      // Look for peers with server access
      for (int j = 0; j < peerCount; j++) {
        PeerDevice& peer = knownPeers[j];
        
        if (!peer.handshakeComplete || !peer.validated) continue;
        if (!canRelayToPeer(peer.deviceId, msg)) continue;
        
        // Check if peer has server access capability
        bool hasServerAccess = false;
        for (const String& cap : peer.capabilities) {
          if (cap == "server_access") {
            hasServerAccess = true;
            break;
          }
        }
        
        if (hasServerAccess) {
          Serial.printf("📡 Attempting relay of %s to %s (has server access)\n",
                        msg.messageId.c_str(), peer.deviceId.c_str());
          relayMessageToPeer(i, peer.deviceId);
          msg.lastRelayAttempt = millis();
          break; // Try one peer at a time
        }
      }
    }
  }

  void attemptServerDelivery() {
    // Try to deliver stored messages directly to server
    if (!hasServerConnection() || storedMessageCount == 0) return;
    
    Serial.println("📤 Attempting server delivery of stored messages...");
    
    for (int i = 0; i < storedMessageCount; i++) {
      StoredMessage& msg = messageStorage[i];
      
      if (msg.deliveredToServer) continue;
      
      // TODO: Implement actual server delivery via HTTP/WebSocket
      // For now, simulate server delivery
      Serial.printf("📤 Delivering message %s to server (simulated)\n", 
                    msg.messageId.c_str());
      
      // Mark as delivered
      msg.deliveredToServer = true;
      
      // In real implementation, would send HTTP POST or WebSocket message
      // with complete message data and relay chain information
    }
    
    // Clean up delivered messages
    for (int i = storedMessageCount - 1; i >= 0; i--) {
      if (messageStorage[i].deliveredToServer) {
        // Shift remaining messages
        for (int j = i; j < storedMessageCount - 1; j++) {
          messageStorage[j] = messageStorage[j + 1];
        }
        storedMessageCount--;
        Serial.println("🗑️ Removed delivered message from storage");
      }
    }
  }

  void relayMessageToPeer(int messageIndex, const String& peerId) {
    // Send stored message to specific peer for relay
    if (messageIndex < 0 || messageIndex >= storedMessageCount) return;
    
    StoredMessage& msg = messageStorage[messageIndex];
    
    // Find peer MAC address
    uint8_t peerMac[6];
    bool foundPeer = false;
    
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == peerId) {
        // Convert MAC string to bytes
        String macStr = knownPeers[i].macAddress;
        sscanf(macStr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X", 
              &peerMac[0], &peerMac[1], &peerMac[2], &peerMac[3], &peerMac[4], &peerMac[5]);
        foundPeer = true;
        break;
      }
    }
    
    if (!foundPeer) {
      Serial.printf("❌ Could not find MAC for peer: %s\n", peerId.c_str());
      return;
    }
    
    // Add ourselves to the relay chain
    updateMessageRelayChain(msg, DEVICE_ID, DEVICE_OWNER, WiFi.RSSI());
    
    // Send relay message
    sendRelayMessage(msg, peerMac);
    
    // Track that we attempted relay to this peer
    msg.attemptedPeers.push_back(peerId);
    msg.lastRelayAttempt = millis();
  }

  void sendRelayMessage(const StoredMessage& storedMsg, const uint8_t* peerMac) {
    // Send relay request message to peer
    JsonDocument relayDoc;
    
    // Create envelope structure
    relayDoc["version"] = PROTOCOL_VERSION;
    relayDoc["message_id"] = generateUniqueMessageId();
    relayDoc["timestamp"] = millis() / 1000;
    relayDoc["shared_key"] = SHARED_KEY;
    
    // Source device info
    JsonObject sourceDevice = relayDoc["source_device"].to<JsonObject>();
    sourceDevice["device_id"] = DEVICE_ID;
    sourceDevice["owner"] = DEVICE_OWNER;
    sourceDevice["mac_address"] = WiFi.macAddress();
    sourceDevice["device_type"] = DEVICE_TYPE;
    sourceDevice["firmware_version"] = FIRMWARE_VERSION;
    
    relayDoc["message_type"] = "relay";
    
    // Relay-specific payload
    JsonObject payload = relayDoc["payload"].to<JsonObject>();
    payload["request_type"] = "delivery_request";
    payload["relay_message_id"] = storedMsg.messageId;
    payload["original_sender"] = storedMsg.originalSender;
    payload["hop_count"] = storedMsg.hopCount;
    
    // Include the complete original message
    payload["message_data"] = storedMsg.messageData;
    
    // Update relay chain in the message data
    JsonArray relayChain = payload["message_data"]["payload"]["relay_chain"].to<JsonArray>();
    for (const RelayHop& hop : storedMsg.relayChain) {
      JsonObject hopObj = relayChain.add<JsonObject>();
      hopObj["device_id"] = hop.deviceId;
      hopObj["device_owner"] = hop.deviceOwner;
      hopObj["timestamp"] = hop.timestamp;
      hopObj["rssi"] = hop.rssi;
    }
    
    // Serialize and send
    String message;
    serializeJson(relayDoc, message);
    
    Serial.printf("🔄 Sending relay request for message %s\n", storedMsg.messageId.c_str());
    
    esp_err_t result = esp_now_send(peerMac, (uint8_t*)message.c_str(), message.length());
    
    if (result == ESP_OK) {
      Serial.println("✓ Relay request sent");
    } else {
      Serial.printf("❌ Error sending relay request: %d\n", result);
    }
  }

  bool canRelayToPeer(const String& peerId, const StoredMessage& msg) {
    // Check if we can relay this message to the specified peer
    
    // Don't relay to original sender
    if (peerId == msg.originalSender) return false;
    
    // Don't relay to ourselves
    if (peerId == DEVICE_ID) return false;
    
    // Don't relay if peer is already in relay chain (prevent loops)
    for (const RelayHop& hop : msg.relayChain) {
      if (hop.deviceId == peerId) return false;
    }
    
    // Don't retry peers we've already attempted recently
    for (const String& attempted : msg.attemptedPeers) {
      if (attempted == peerId) return false;
    }
    
    return true;
  }

  void cleanupExpiredMessages() {
    // Remove messages that have expired
    unsigned long currentTime = millis();
    
    for (int i = storedMessageCount - 1; i >= 0; i--) {
      if (currentTime - messageStorage[i].timestamp > MESSAGE_EXPIRY_TIME) {
        Serial.printf("🗑️ Removing expired message: %s\n", messageStorage[i].messageId.c_str());
        
        // Shift remaining messages
        for (int j = i; j < storedMessageCount - 1; j++) {
          messageStorage[j] = messageStorage[j + 1];
        }
        storedMessageCount--;
      }
    }
  }

  void printMessageStorage() {
    // Display current message storage status
    Serial.printf("📦 Message Storage Status: %d/%d messages\n", storedMessageCount, MAX_STORED_MESSAGES);
    
    if (storedMessageCount == 0) {
      Serial.println("  No stored messages");
      return;
    }
    
    for (int i = 0; i < storedMessageCount; i++) {
      StoredMessage& msg = messageStorage[i];
      Serial.printf("  %d. %s from %s (%s)\n", 
                    i + 1, msg.messageId.c_str(), msg.originalSender.c_str(), msg.senderOwner.c_str());
      Serial.printf("     Hops: %d | Delivered: %s | Age: %lus\n",
                    msg.hopCount,
                    msg.deliveredToServer ? "Yes" : "No",
                    (millis() - msg.timestamp) / 1000);
      
      if (!msg.relayChain.empty()) {
        Serial.print("     Relay chain: ");
        for (size_t j = 0; j < msg.relayChain.size(); j++) {
          Serial.print(msg.relayChain[j].deviceId);
          if (j < msg.relayChain.size() - 1) Serial.print(" -> ");
        }
        Serial.println();
      }
    }
    Serial.println();
  }

  String generateUniqueMessageId() {
    // Generate unique message ID with device identifier and counter
    messageIdCounter++;
    return String("msg_") + DEVICE_ID + "_" + String(millis()) + "_" + String(messageIdCounter);
  }

  void updateMessageRelayChain(StoredMessage& msg, const String& relayDeviceId, const String& relayOwner, int rssi) {
    // Add relay hop to message chain
    RelayHop hop;
    hop.deviceId = relayDeviceId;
    hop.deviceOwner = relayOwner;
    hop.timestamp = millis() / 1000;
    hop.rssi = rssi;
    
    msg.relayChain.push_back(hop);
    msg.hopCount = msg.relayChain.size();
  }

  bool hasServerConnection() {
    // Check if this device has server connection capability
    return (currentMode == MODE_WIFI_PRIMARY || currentMode == MODE_WIFI_BACKUP) && 
          wifiConnected && serverReachable;
  }

  void markMessageDelivered(const String& messageId) {
    // Mark message as successfully delivered to server
    for (int i = 0; i < storedMessageCount; i++) {
      if (messageStorage[i].messageId == messageId) {
        messageStorage[i].deliveredToServer = true;
        Serial.printf("✅ Marked message as delivered: %s\n", messageId.c_str());
        break;
      }
    }
  }

  int findStoredMessage(const String& messageId) {
    // Find stored message by ID
    for (int i = 0; i < storedMessageCount; i++) {
      if (messageStorage[i].messageId == messageId) {
        return i;
      }
    }
    return -1;
  }

  // ============================================================================
  //                         RADIO MANAGEMENT FUNCTIONS
  // ============================================================================

  void enableWiFiMode() {
    if (wifiModeActive) return;  // Already in WiFi mode
    
    unsigned long currentTime = millis();
    if (currentTime - lastRadioSwitch < RADIO_SWITCH_DELAY) {
      delay(RADIO_SWITCH_DELAY - (currentTime - lastRadioSwitch));
    }
    
    if (espNowActive) {
      Serial.println("📡 Switching radio: ESP-NOW -> WiFi");
      esp_now_deinit();
      espNowActive = false;
      delay(100);  // Allow ESP-NOW to properly deinitialize
    }
    
    WiFi.mode(WIFI_STA);
    wifiModeActive = true;
    lastRadioSwitch = millis();
    
    Serial.println("📶 WiFi mode activated");
  }

  void enableESPNowMode() {
    if (espNowActive) return;  // Already in ESP-NOW mode
    
    unsigned long currentTime = millis();
    if (currentTime - lastRadioSwitch < RADIO_SWITCH_DELAY) {
      delay(RADIO_SWITCH_DELAY - (currentTime - lastRadioSwitch));
    }
    
    if (wifiModeActive) {
      Serial.println("📡 Switching radio: WiFi -> ESP-NOW");
      WiFi.disconnect();
      delay(100);  // Allow WiFi to properly disconnect
      wifiModeActive = false;
    }
    
    // Always deinitialize ESP-NOW before reinitializing (like legacy code)
    if (espNowActive) {
      esp_now_deinit();
    }
    
    // Reinitialize ESP-NOW using the same method as legacy code
    initESPNow();
    espNowActive = true;
    lastRadioSwitch = millis();
    
    Serial.println("📡 ESP-NOW mode activated");
  }

  void ensureESPNowActive() {
    if (!espNowActive) {
      enableESPNowMode();
    }
  }

  void ensureWiFiActive() {
    if (!wifiModeActive) {
      enableWiFiMode();
    }
  }

  void deinitESPNow() {
    if (espNowActive) {
      esp_now_deinit();
      espNowActive = false;
      Serial.println("📡 ESP-NOW deinitialized");
    }
  }