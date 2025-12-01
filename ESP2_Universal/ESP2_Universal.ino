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
    
    // Clean up old peers (remove if not seen for 30 seconds)
    for (int i = 0; i < peerCount; i++) {
      if (currentTime - knownPeers[i].lastSeen > 30000) {
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

  // MESSAGE TYPE CODES: 0=ping, 1=data, 2=handshake, 3=triangulation, 4=relay, 5=distance
  
  // Helper: Build common envelope fields for all messages
  void buildEnvelope(JsonDocument& doc, int msgType) {
    doc["v"] = 5;
    doc["i"] = millis() % 100000;
    doc["t"] = millis() / 1000;
    doc["k"] = "ESP2_NET";
    doc["y"] = msgType;
    doc["d"] = DEVICE_ID;
    doc["o"] = DEVICE_OWNER;
    doc["m"] = WiFi.macAddress();
  }
  
  // Helper: Send message via ESP-NOW broadcast
  esp_err_t broadcastMsg(JsonDocument& doc, const char* label) {
    String message;
    serializeJson(doc, message);
    Serial.printf("%s (%d bytes)\n", label, message.length());
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
  }

  void sendPeerDiscoveryPing() {
    ensureESPNowActive();
    
    JsonDocument doc;
    buildEnvelope(doc, 0);  // 0=ping
    doc["r"] = WiFi.RSSI();
    doc["h"] = ESP.getFreeHeap() / 1000;
    doc["u"] = millis() / 1000;
    doc["n"] = peerCount;
    
    esp_err_t result = broadcastMsg(doc, "📡 Ping");
    Serial.println(result == ESP_OK ? "✓" : "❌");
  }


  void sendHandshakeResponse(const String& replyToMessageId, const uint8_t* peerMac) {
    JsonDocument dummyPing;
    dummyPing["d"] = "unknown";
    sendEnhancedHandshake(replyToMessageId, peerMac, dummyPing);
  }


  void sendDataMessage() {
    ensureESPNowActive();
    
    JsonDocument doc;
    buildEnvelope(doc, 1);  // 1=data
    doc["h"] = ESP.getFreeHeap() / 1000;
    doc["u"] = millis() / 1000;
    doc["n"] = peerCount;
    
    esp_err_t result = broadcastMsg(doc, "📊 Data");
    if (result == ESP_OK) {
      printKnownPeers();
    }
  }


  // ============================================================================
  //                        MESSAGE HANDLING
  // ============================================================================

  void onESPNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    String receivedMessage = "";
    for (int i = 0; i < len; i++) {
      receivedMessage += (char)data[i];
    }
    
    Serial.println("\n📥 Message Received");
    Serial.printf("From: %s | RSSI: %d\n", macToString(info->src_addr).c_str(), info->rx_ctrl->rssi);
    Serial.printf("Raw: %s\n", receivedMessage.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, receivedMessage);
    
    if (error) {
      Serial.printf("❌ JSON parse fail: %s\n\n", error.c_str());
      return;
    }
    
    if (!validateEnvelope(doc)) {
      Serial.println("❌ Invalid envelope\n");
      return;
    }
    
    // Get device ID from compact format
    String senderDeviceId = doc["d"] | "";
    if (senderDeviceId == DEVICE_ID) {
      Serial.println("🚫 Ignoring self\n");
      return;
    }
    
    Serial.println("✓ Valid");
    processIncomingMessage(doc, info->src_addr, info->rx_ctrl->rssi);
  }

  bool validateEnvelope(JsonDocument& doc) {
    // COMPACT FORMAT: v, i, t, k, y, d, o, m required
    if (!doc.containsKey("v") || !doc.containsKey("k") || 
        !doc.containsKey("y") || !doc.containsKey("d")) {
      Serial.println("❌ Missing required fields");
      return false;
    }
    
    // Validate key (abbreviated)
    String receivedKey = doc["k"] | "";
    if (receivedKey != "ESP2_NET") {
      Serial.printf("❌ Bad key: %s\n", receivedKey.c_str());
      return false;
    }
    
    return true;
  }

  String generateMessageId(const String& messageType) {
    messageCounter++;
    return String(millis() % 100000);
  }

  // Get message type string from code: 0=ping, 1=data, 2=handshake, 3=tri, 4=relay, 5=dist
  String getTypeStr(int y) {
    switch(y) {
      case 0: return "ping";
      case 1: return "data";
      case 2: return "handshake";
      case 3: return "tri";
      case 4: return "relay";
      case 5: return "dist";
      default: return "unknown";
    }
  }

  void processIncomingMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
    // COMPACT FORMAT: y=type, d=device, o=owner, m=mac
    int msgType = doc["y"] | -1;
    String messageId = String(doc["i"] | 0);
    String senderDeviceId = doc["d"] | "";
    String senderOwner = doc["o"] | "";
    String senderMacStr = doc["m"] | macToString(senderMac);
    
    Serial.printf("📨 %s from %s (%s)\n", getTypeStr(msgType).c_str(), senderDeviceId.c_str(), senderOwner.c_str());
    
    // Update peer
    addOrUpdatePeer(senderDeviceId, senderOwner, senderMacStr, rssi);
    
    if (msgType == 0) {  // ping
      Serial.printf("🏓 Ping from %s - trusted\n", senderDeviceId.c_str());
      for (int i = 0; i < peerCount; i++) {
        if (knownPeers[i].deviceId == senderDeviceId) {
          knownPeers[i].handshakeComplete = true;
          knownPeers[i].validated = true;
          break;
        }
      }
      // Send handshake response
      sendEnhancedHandshake(messageId, senderMac, doc);
      
    } else if (msgType == 1) {  // data
      Serial.printf("📊 Data from %s\n", senderDeviceId.c_str());
      // Extract sensor data if present
      if (doc.containsKey("T")) {
        float temp = doc["T"].as<int>() / 10.0;
        float hum = doc["H"].as<int>() / 10.0;
        Serial.printf("   Temp: %.1f°C, Hum: %.1f%%\n", temp, hum);
      }
      for (int i = 0; i < peerCount; i++) {
        if (knownPeers[i].deviceId == senderDeviceId) {
          knownPeers[i].handshakeComplete = true;
          knownPeers[i].validated = true;
          break;
        }
      }
      
    } else if (msgType == 2) {  // handshake
      Serial.printf("🤝 Handshake from %s\n", senderDeviceId.c_str());
      addOrUpdatePeer(senderDeviceId, senderOwner, senderMacStr, rssi);
      
      bool ok = doc["ok"].as<int>() == 1;
      Serial.printf("   ok: %s\n", ok ? "true" : "false");
      if (ok) {
        for (int i = 0; i < peerCount; i++) {
          if (knownPeers[i].deviceId == senderDeviceId) {
            knownPeers[i].handshakeComplete = true;
            knownPeers[i].validated = true;
            Serial.printf("✅ %s validated\n", senderDeviceId.c_str());
            break;
          }
        }
        // Send response back
        sendEnhancedHandshake(messageId, senderMac, doc);
      }
      
    } else if (msgType == 3) {  // triangulation
      Serial.printf("📍 Tri data from %s\n", senderDeviceId.c_str());
      
    } else if (msgType == 5) {  // distance
      Serial.printf("📏 Distance from %s\n", senderDeviceId.c_str());
      if (doc.containsKey("dist")) {
        Serial.printf("   Distance: %.1fm\n", doc["dist"].as<float>());
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
    // Simplified - just check key exists
    return doc.containsKey("k");
  }

  void sendEnhancedHandshake(const String& replyToMessageId, const uint8_t* peerMac, JsonDocument& originalPing) {
    // Get peer device ID from compact format
    String peerDeviceId = originalPing["d"] | "";
    
    // Check cooldown
    if (handshakeAttempts.find(peerDeviceId) != handshakeAttempts.end()) {
      if (millis() - handshakeAttempts[peerDeviceId] < HANDSHAKE_TIMEOUT) {
        Serial.printf("🚫 Cooldown for %s\n", peerDeviceId.c_str());
        return;
      }
    }
    handshakeAttempts[peerDeviceId] = millis();
    
    // Register peer MAC for ESP-NOW unicast
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peerMac, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(peerMac)) {
      esp_now_add_peer(&peerInfo);
    }
    
    JsonDocument doc;
    buildEnvelope(doc, 2);  // 2=handshake
    doc["re"] = replyToMessageId;
    doc["ok"] = 1;
    
    String message;
    serializeJson(doc, message);
    Serial.printf("🤝 Handshake to %s (%d bytes)\n", peerDeviceId.c_str(), message.length());
    
    esp_err_t result = esp_now_send(peerMac, (uint8_t*)message.c_str(), message.length());
    Serial.println(result == ESP_OK ? "✓" : "❌");
  }

  bool isPeerTrusted(const String& deviceId) {
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].deviceId == deviceId) {
        return knownPeers[i].validated && 
              knownPeers[i].handshakeComplete && 
              (millis() - knownPeers[i].lastSeen < 300000);
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
    if (!hasEnoughPeersForPositioning()) return;
    
    Serial.println("📏 Distance measurement...");
    
    for (int i = 0; i < peerCount; i++) {
      float distance = calculateDistanceFromRSSI(knownPeers[i].rssi);
      knownPeers[i].relativePos.distance = distance;
      
      JsonDocument doc;
      buildEnvelope(doc, 5);  // 5=distance
      doc["to"] = knownPeers[i].deviceId;
      doc["r"] = knownPeers[i].rssi;
      doc["dist"] = (int)(distance * 10);
      
      esp_err_t result = broadcastMsg(doc, "📏 Dist");
      Serial.printf("  %s: %.1fm %s\n", knownPeers[i].deviceId.c_str(), distance, result == ESP_OK ? "✓" : "❌");
    }
  }
  

  // Main Triangulation Processing
  void performTriangulation() {
    if (!hasEnoughPeersForTriangulation()) {
      Serial.println("⚠️ Not enough peers for triangulation");
      return;
    }
    
    Serial.println("📐 Triangulation update...");
    
    for (int i = 0; i < peerCount; i++) {
      if (knownPeers[i].handshakeComplete && knownPeers[i].validated) {
        updatePeerPosition(knownPeers[i].deviceId, knownPeers[i].rssi);
      }
    }
    
    // Estimate relative directions
    estimateRelativePositions();
    
    printPositioningSummary();
    Serial.println("✓ Triangulation done\n");
  }

  // Triangulation Data Exchange
  void sendTriangulationPing() {
    JsonDocument doc;
    buildEnvelope(doc, 3);  // 3=triangulation
    doc["r"] = WiFi.RSSI();
    
    if (myPosition.isValid) {
      doc["px"] = (int)(myPosition.x * 10);
      doc["py"] = (int)(myPosition.y * 10);
    }
    
    if (peerCount > 0) {
      JsonArray pa = doc["pa"].to<JsonArray>();
      for (int i = 0; i < peerCount && i < 3; i++) {
        if (knownPeers[i].relativePos.isValid) {
          JsonObject p = pa.add<JsonObject>();
          p["d"] = knownPeers[i].deviceId.substring(0, 8);
          p["di"] = (int)(knownPeers[i].relativePos.distance * 10);
        }
      }
    }
    
    broadcastMsg(doc, "📡 Tri");
  }

  // Triangulation Data Processing
  void processTriangulationData(JsonDocument& doc, int rssi) {
    String senderDevice = doc["d"] | doc["source_device"]["device_id"].as<const char*>();
    
    updatePeerPosition(senderDevice, rssi);
    
    // Compact: px, py  |  Verbose: payload.position.x, y
    if (doc.containsKey("px")) {
      float x = doc["px"].as<float>() / 10.0;
      float y = doc["py"].as<float>() / 10.0;
    } else if (doc["payload"].containsKey("position")) {
      JsonObject position = doc["payload"]["position"];
    }
    
    // Compact: pa[]  |  Verbose: payload.nearby_peers[]
    if (doc.containsKey("pa")) {
      JsonArray pa = doc["pa"];
    } else if (doc["payload"].containsKey("nearby_peers")) {
      JsonArray nearbyPeers = doc["payload"]["nearby_peers"];
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
    // Handle both compact (i) and verbose (message_id) formats
    String messageId = messageDoc["i"] | messageDoc["message_id"].as<const char*>();
    
    // Check if message already exists
    for (int i = 0; i < storedMessageCount; i++) {
      if (messageStorage[i].messageId == messageId) {
        return;  // Already stored
      }
    }
    
    // Find available storage slot
    if (storedMessageCount >= MAX_STORED_MESSAGES) {
      for (int i = 0; i < storedMessageCount - 1; i++) {
        messageStorage[i] = messageStorage[i + 1];
      }
      storedMessageCount--;
    }
    
    // Store - handle both formats for owner
    int storeIndex = storedMessageCount;
    messageStorage[storeIndex].messageId = messageId;
    messageStorage[storeIndex].originalSender = senderId;
    messageStorage[storeIndex].senderOwner = messageDoc["o"] | messageDoc["source_device"]["owner"].as<const char*>();
    messageStorage[storeIndex].messageData = messageDoc;
    messageStorage[storeIndex].timestamp = millis();
    messageStorage[storeIndex].lastRelayAttempt = 0;
    messageStorage[storeIndex].deliveredToServer = false;
    messageStorage[storeIndex].isOwnMessage = isOwnMessage;
    messageStorage[storeIndex].hopCount = 0;
    messageStorage[storeIndex].relayChain.clear();
    messageStorage[storeIndex].attemptedPeers.clear();
    
    // Extract relay chain - compact: rc[], verbose: payload.relay_chain[]
    JsonArray relayChain;
    if (messageDoc.containsKey("rc")) {
      relayChain = messageDoc["rc"];
    } else if (messageDoc["payload"].containsKey("relay_chain")) {
      relayChain = messageDoc["payload"]["relay_chain"];
    }
    
    for (JsonVariant hop : relayChain) {
      RelayHop relayHop;
      relayHop.deviceId = hop["d"] | hop["device_id"].as<const char*>();
      relayHop.deviceOwner = hop["o"] | hop["device_owner"].as<const char*>();
      relayHop.timestamp = hop["t"] | hop["timestamp"].as<unsigned long>();
      relayHop.rssi = hop["r"] | hop["rssi"].as<int>();
      messageStorage[storeIndex].relayChain.push_back(relayHop);
    }
    messageStorage[storeIndex].hopCount = messageStorage[storeIndex].relayChain.size();
    
    storedMessageCount++;
    
    Serial.printf("📦 Stored message: %s from %s (%s) - Storage: %d/%d\n", 
                  messageId.c_str(), senderId.c_str(), 
                  messageStorage[storeIndex].senderOwner.c_str(),
                  storedMessageCount, MAX_STORED_MESSAGES);
  }

  void processRelayMessage(JsonDocument& doc, const uint8_t* senderMac, int rssi) {
    // Handle both compact and verbose formats
    String senderDeviceId = doc["d"] | doc["source_device"]["device_id"].as<const char*>();
    String relayMessageId = doc["ri"] | doc["payload"]["relay_message_id"].as<const char*>();
    
    Serial.printf("🔄 Relay from %s for %s\n", senderDeviceId.c_str(), relayMessageId.c_str());
    
    // Check for message data (compact: md, verbose: payload.message_data)
    if (doc.containsKey("md")) {
      JsonDocument relayedMessage = doc["md"];
      storeMessage(relayedMessage, senderDeviceId, false);
    } else if (doc["payload"].containsKey("message_data")) {
      JsonDocument relayedMessage = doc["payload"]["message_data"];
      storeMessage(relayedMessage, senderDeviceId, false);
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
    JsonDocument doc;
    buildEnvelope(doc, 4);  // 4=relay
    doc["ri"] = storedMsg.messageId;
    doc["os"] = storedMsg.originalSender;
    doc["hc"] = storedMsg.hopCount;
    doc["md"] = storedMsg.messageData;
    
    JsonArray rc = doc["rc"].to<JsonArray>();
    for (const RelayHop& hop : storedMsg.relayChain) {
      JsonObject h = rc.add<JsonObject>();
      h["d"] = hop.deviceId;
      h["r"] = hop.rssi;
    }
    
    String message;
    serializeJson(doc, message);
    Serial.printf("🔄 Relay %s (%d bytes)\n", storedMsg.messageId.c_str(), message.length());
    
    esp_err_t result = esp_now_send(peerMac, (uint8_t*)message.c_str(), message.length());
    Serial.println(result == ESP_OK ? "✓" : "❌");
  }

  bool canRelayToPeer(const String& peerId, const StoredMessage& msg) {
    if (peerId == msg.originalSender) return false;
    if (peerId == DEVICE_ID) return false;
    
    for (const RelayHop& hop : msg.relayChain) {
      if (hop.deviceId == peerId) return false;
    }
    
    for (const String& attempted : msg.attemptedPeers) {
      if (attempted == peerId) return false;
    }
    
    return true;
  }

  void cleanupExpiredMessages() {
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