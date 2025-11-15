/*
 * ESP1 - UNIVERSAL MESH RELAY (Always-On ESP-NOW Hub)
 * 
 * ESP1 is a UNIVERSAL relay for ANY ESP32 in the mesh network:
 * - Receives ESP-NOW from ALL ESP32 devices on channel 1
 * - Acts as mesh hub - any ESP can reach Electron through ESP1
 * - ESP-NOW always active regardless of USB/WiFi transport mode
 * 
 * AUTOMATIC TRANSPORT SWITCHING:
 * - Connected to PC/Laptop (USB Serial active) → USB Transport
 *   Forwards all mesh ESP-NOW messages to Electron via USB Serial
 * 
 * - Connected to Power Supply only → WiFi Transport
 *   Connects to WiFi, forwards all mesh messages via WebSocket
 * 
 * This is the MESH HUB - any ESP32 can use ESP-NOW to reach Electron!
 * 
 * Hardware: Any ESP32 board with WiFi and USB capability
 * 
 * Setup Instructions:
 * 1. Install required libraries:
 *    - ESP32 Board Support (via Board Manager)
 *    - ArduinoWebsockets (by Gil Maimon) - via Library Manager
 *    - ArduinoJson (by Benoit Blanchon) - via Library Manager
 * 2. Update WiFi credentials (for relay mode)
 * 3. Update WebSocket server IP (for relay mode)
 * 4. Upload to ESP32
 * 
 * Home WiFi → use 192.168.1.4
 * 
 * Laptop hotspot → use 192.168.137.1
 * 
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// ============ CONFIGURATION ============

// WiFi Configuration (for Relay Mode)
const char* WIFI_SSID = "YOUR_WIFI_SSID";          // ⚠️ Change this to your WiFi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // ⚠️ Change this to your WiFi password

// WebSocket Configuration (for Relay Mode)
const char* WEBSOCKET_SERVER_IP = "192.168.1.4";   // ⚠️ Change to your PC's IP (use ipconfig to find it)
const uint16_t WEBSOCKET_SERVER_PORT = 8080;        // ✓ Must match Electron app port

// Device Configuration
const char* DEVICE_ID = "ESP1_MAIN";

// ============ MODE DETECTION ============

enum OperationMode {
  MODE_USB,       // Connected to PC - USB Serial only
  MODE_RELAY,     // Power only - WiFi Relay only
  MODE_DUAL       // USB + WiFi - Both transports active
};

OperationMode currentMode;

// ============ SHARED VARIABLES ============

int receiveMessageCount = 0;

// ============ USB MODE VARIABLES ============

unsigned long lastStatusSend = 0;
const unsigned long STATUS_SEND_INTERVAL = 3000;

// ============ RELAY MODE VARIABLES ============

WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 2000;

int messageCount = 0;

// =======================================

void setup() {
  Serial.begin(115200);
  delay(2000); // Give time for Serial to initialize
  
  Serial.println("\n=============================================");
  Serial.println("ESP1 - Smart Dual-Mode ESP32");
  Serial.println("=============================================\n");

  // Detect operation mode
  detectMode();
  
  // Common setup
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Set WiFi channel to 1 (ESP-NOW requires same channel)
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  Serial.println("WiFi channel set to 1 for ESP-NOW");
  
  // Initialize ESP-NOW (both modes need it)
  initESPNow();
  
  // Mode-specific initialization
  if (currentMode == MODE_USB) {
    setupUSBMode();
  } else if (currentMode == MODE_RELAY) {
    setupRelayMode();
  } else {
    setupDualMode();
  }
}

void loop() {
  if (currentMode == MODE_USB) {
    loopUSBMode();
  } else if (currentMode == MODE_RELAY) {
    loopRelayMode();
  } else {
    loopDualMode();
  }
  
  delay(10);
}

// ============ MODE DETECTION ============

void detectMode() {
  // Check if USB Serial is connected to a PC
  bool hasUSB = false;
  if (Serial) {
    delay(100);
    if (Serial.availableForWrite() > 0) {
      hasUSB = true;
    }
  }
  
  if (hasUSB) {
    // USB detected - try to enable WiFi as well for dual transport
    currentMode = MODE_DUAL;
    Serial.println("✓ USB CONNECTION DETECTED");
    Serial.println("Transport: DUAL MODE (USB + WiFi)");
    Serial.println("ESP-NOW mesh hub active → forwarding ALL ESPs via BOTH USB and WiFi\n");
  } else {
    // No USB connection detected - use WiFi relay mode only
    currentMode = MODE_RELAY;
    Serial.println("✓ POWER SUPPLY DETECTED (No USB)");
    Serial.println("Transport: WiFi WebSocket");
    Serial.println("ESP-NOW mesh hub active → forwarding ALL ESPs via WebSocket\n");
  }
}

// ============ ESP-NOW INITIALIZATION ============

void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  Serial.println("ESP-NOW initialized successfully");
  
  // Register receive callback
  esp_now_register_recv_cb(onESPNowDataReceived);
}

// ============ USB MODE ============

void setupUSBMode() {
  Serial.println("\n--- USB Mode Setup ---");
  Serial.println("ESP-NOW receive mode enabled");
  
  sendUSBStatus("ESP1 MESH HUB - USB transport initialized");
  
  Serial.println("\nReady! This MESH HUB will:");
  Serial.println("  1. Receive ESP-NOW from ANY ESP32 on channel 1");
  Serial.println("  2. Forward all mesh messages to Electron via USB Serial");
  Serial.println("  3. Act as universal relay - any ESP can reach Electron\n");
}

void loopUSBMode() {
  // Send periodic status updates
  if (millis() - lastStatusSend > STATUS_SEND_INTERVAL) {
    lastStatusSend = millis();
    sendUSBPeriodicStatus();
  }
}

// ============ DUAL MODE ============

void setupDualMode() {
  Serial.println("\n--- DUAL MODE Setup ---");
  Serial.println("Enabling both USB Serial and WiFi WebSocket transports");
  
  // Initialize WiFi
  initWiFi();
  
  // Connect to WebSocket
  connectWebSocket();
  
  Serial.println("ESP-NOW receive mode enabled");
  sendUSBStatus("ESP1 MESH HUB - DUAL transport initialized");
  
  Serial.println("\nReady! This MESH HUB will:");
  Serial.println("  1. Receive ESP-NOW from ANY ESP32 on channel 1");
  Serial.println("  2. Forward ALL mesh messages via USB Serial");
  Serial.println("  3. Forward ALL mesh messages via WiFi WebSocket");
  Serial.println("  4. Test both transport paths with single ESP-NOW message\n");
}

void loopDualMode() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    initWiFi();
  }

  // Maintain WebSocket connection
  if (wsConnected) {
    wsClient.poll();
  } else {
    // Try to reconnect
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      connectWebSocket();
    }
  }

  // Send periodic status updates via USB
  if (millis() - lastStatusSend > STATUS_SEND_INTERVAL) {
    lastStatusSend = millis();
    sendDualPeriodicStatus();
  }
  
  // Send periodic data via WiFi
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendDualWiFiData();
  }
}

void sendDualPeriodicStatus() {
  // Send via USB
  StaticJsonDocument<400> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "DUAL";
  doc["received_count"] = receiveMessageCount;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["ws_connected"] = wsConnected;
  doc["message"] = "ESP1 MESH HUB (DUAL) - Receiving from all ESPs";
  
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println(jsonString);
}

void sendDualWiFiData() {
  if (!wsConnected) return;

  messageCount++;
  
  StaticJsonDocument<300> doc;
  doc["source"] = "WIFI";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "DUAL";
  doc["message_count"] = messageCount;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["message"] = "ESP1 MESH HUB (DUAL) - Relaying all ESPs";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  wsClient.send(jsonString);
  Serial.println("Sent WiFi data: " + jsonString);
}

void sendDualReceivedData(String senderMAC, String data, int dataLength) {
  // Send via USB
  StaticJsonDocument<500> usbDoc;
  usbDoc["source"] = "USB";
  usbDoc["device_id"] = DEVICE_ID;
  usbDoc["mode"] = "DUAL";
  usbDoc["sender_mac"] = senderMAC;
  usbDoc["received_data"] = data;
  usbDoc["data_length"] = dataLength;
  usbDoc["receive_count"] = receiveMessageCount;
  usbDoc["uptime"] = millis() / 1000;
  usbDoc["message"] = "ESP-NOW mesh message (via USB path)";
  
  String usbJson;
  serializeJson(usbDoc, usbJson);
  Serial.println(usbJson);
  
  // Send via WiFi if connected
  if (wsConnected) {
    StaticJsonDocument<400> wifiDoc;
    wifiDoc["source"] = "RELAY";
    wifiDoc["device_id"] = DEVICE_ID;
    wifiDoc["mode"] = "DUAL";
    wifiDoc["sender_mac"] = senderMAC;
    wifiDoc["relayed_data"] = data;
    wifiDoc["rssi"] = WiFi.RSSI();
    wifiDoc["message"] = "Mesh relay from " + senderMAC + " (via WiFi path)";
    
    String wifiJson;
    serializeJson(wifiDoc, wifiJson);
    wsClient.send(wifiJson);
    Serial.println("Relayed via WiFi: " + wifiJson);
  }
}

// ============ USB MODE ============

void sendUSBPeriodicStatus() {
  StaticJsonDocument<400> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "USB";
  doc["received_count"] = receiveMessageCount;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["message"] = "ESP1 MESH HUB (USB) - Receiving from all ESPs";
  
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println(jsonString);
}

void sendUSBStatus(String statusMessage) {
  StaticJsonDocument<200> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "USB";
  doc["message"] = statusMessage;
  doc["uptime"] = millis() / 1000;
  
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println(jsonString);
}

void sendUSBReceivedData(String senderMAC, String data, int dataLength) {
  StaticJsonDocument<500> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "USB";
  doc["sender_mac"] = senderMAC;
  doc["received_data"] = data;
  doc["data_length"] = dataLength;
  doc["receive_count"] = receiveMessageCount;
  doc["uptime"] = millis() / 1000;
  doc["message"] = "ESP-NOW mesh message received via USB path";
  
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println(jsonString);
}

// ============ RELAY MODE ============

void setupRelayMode() {
  Serial.println("\n--- WiFi Relay Mode Setup ---");
  
  // Connect to WiFi
  initWiFi();
  
  // Connect to WebSocket
  connectWebSocket();
  
  Serial.println("\nReady! This MESH HUB will:");
  Serial.println("  1. Receive ESP-NOW from ANY ESP32 on channel 1");
  Serial.println("  2. Forward all mesh messages to Electron via WebSocket");
  Serial.println("  3. Send periodic WiFi status updates");
  Serial.println("  4. Act as universal relay - any ESP can reach Electron\n");
}

void loopRelayMode() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    initWiFi();
  }

  // Maintain WebSocket connection
  if (wsConnected) {
    wsClient.poll();
  } else {
    // Try to reconnect
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      connectWebSocket();
    }
  }

  // Send periodic data to Electron
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendRelayWiFiData();
  }
}

void initWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  // Connect to WiFi on channel 1 for ESP-NOW compatibility
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 1);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Check actual channel before forcing
    uint8_t primary;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&primary, &secondary);
    Serial.print("⚠️ WiFi connected on channel: ");
    Serial.println(primary);
    
    // Keep channel locked to 1 for ESP-NOW
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    
    // Verify channel was set
    esp_wifi_get_channel(&primary, &secondary);
    Serial.print("✓ Forced WiFi to channel: ");
    Serial.println(primary);
    
    if (primary != 1) {
      Serial.println("❌ CRITICAL: Could not force channel to 1!");
      Serial.println("   Router MUST be on channel 1 for ESP-NOW to work!");
      Serial.println("   ESP-NOW messages will NOT be received!");
    } else {
      Serial.println("✓ ESP-NOW relay active on channel 1");
    }
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("⚠️ Your router must be on WiFi channel 1 for this to work");
    Serial.println("   Check router settings and set 2.4GHz channel to 1");
    Serial.println("✓ ESP-NOW mesh hub still active - can receive from ANY ESP");
  }
}

void connectWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot connect to WebSocket.");
    return;
  }

  String wsUrl = "ws://" + String(WEBSOCKET_SERVER_IP) + ":" + String(WEBSOCKET_SERVER_PORT);
  Serial.print("Connecting to WebSocket: ");
  Serial.println(wsUrl);

  wsConnected = wsClient.connect(wsUrl.c_str());

  if (wsConnected) {
    Serial.println("WebSocket connected!");
    
    // Send initial connection message
    StaticJsonDocument<200> doc;
    doc["source"] = "WIFI";
    doc["device_id"] = DEVICE_ID;
    doc["mode"] = "RELAY";
    doc["message"] = "ESP1 MESH HUB connected - ready to relay all ESPs";
    doc["rssi"] = WiFi.RSSI();
    
    String jsonString;
    serializeJson(doc, jsonString);
    wsClient.send(jsonString);
    
    // Setup WebSocket callbacks
    wsClient.onMessage([](WebsocketsMessage message) {
      Serial.print("Received message from server: ");
      Serial.println(message.data());
    });
    
    wsClient.onEvent([](WebsocketsEvent event, String data) {
      if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WebSocket connection closed");
        wsConnected = false;
      }
    });
  } else {
    Serial.println("WebSocket connection failed!");
    wsConnected = false;
  }
}

void sendRelayWiFiData() {
  if (!wsConnected) return;

  messageCount++;
  
  StaticJsonDocument<300> doc;
  doc["source"] = "WIFI";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "RELAY";
  doc["message_count"] = messageCount;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["message"] = "ESP1 MESH HUB (WiFi) - Relaying all ESPs";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  wsClient.send(jsonString);
  Serial.println("Sent WiFi data: " + jsonString);
}

void relayToElectron(String senderMAC, String data) {
  if (!wsConnected) {
    Serial.println("WebSocket not connected. Cannot relay data.");
    return;
  }

  StaticJsonDocument<400> doc;
  doc["source"] = "RELAY";
  doc["device_id"] = DEVICE_ID;
  doc["mode"] = "RELAY";
  doc["sender_mac"] = senderMAC;
  doc["relayed_data"] = data;
  doc["rssi"] = WiFi.RSSI();
  doc["message"] = "Mesh relay from " + senderMAC;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  wsClient.send(jsonString);
  Serial.println("Relayed to Electron: " + jsonString);
}

// ============ SHARED ESP-NOW CALLBACKS ============

void onESPNowDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  receiveMessageCount++;
  
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  
  String receivedData = "";
  for (int i = 0; i < data_len; i++) {
    receivedData += (char)data[i];
  }
  
  Serial.println("\n🔴🔴🔴 ESP-NOW Message Received! 🔴🔴🔴");
  Serial.print("From MAC: ");
  Serial.println(macStr);
  Serial.print("Data Length: ");
  Serial.println(data_len);
  Serial.print("Data: ");
  Serial.println(receivedData);
  Serial.print("Current Mode: ");
  Serial.println(currentMode == MODE_USB ? "USB" : (currentMode == MODE_RELAY ? "RELAY" : "DUAL"));
  
  // Forward based on current mode
  if (currentMode == MODE_USB) {
    sendUSBReceivedData(macStr, receivedData, data_len);
  } else if (currentMode == MODE_RELAY) {
    relayToElectron(macStr, receivedData);
  } else {
    // DUAL mode - send via both transports
    sendDualReceivedData(macStr, receivedData, data_len);
  }
}

// ============ UTILITY ============

String macToString(uint8_t* mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}
