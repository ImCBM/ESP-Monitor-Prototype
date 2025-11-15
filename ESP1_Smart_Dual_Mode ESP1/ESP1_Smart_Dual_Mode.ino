/*
 * ESP1 - ESP-NOW TO MONITOR RELAY
 * 
 * Simple two-mode operation:
 * 
 * USB MODE (plugged into laptop):
 * - Receives ESP-NOW messages only
 * - Forwards to monitor via USB Serial
 * - NO WiFi (avoids channel conflicts)
 * 
 * WIFI MODE (plain power supply):
 * - Receives ESP-NOW messages
 * - Connects to WiFi
 * - Forwards to monitor via WebSocket
 * 
 * Hardware: Any ESP32 board
 * 
 * Setup:
 * 1. Install libraries:
 *    - ESP32 Board Support
 *    - ArduinoWebsockets (by Gil Maimon)
 *    - ArduinoJson (by Benoit Blanchon)
 * 2. Update WiFi credentials below (for WiFi mode)
 * 3. Update WebSocket server IP below (for WiFi mode)
 * 4. Upload to ESP32
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// ============ CONFIGURATION ============

// WiFi Configuration (for WiFi Mode only)
const char* WIFI_SSID = "YOUR_HOTSPOT_NAME";
const char* WIFI_PASSWORD = "YOUR_HOTSPOT_PASSWORD";

// WebSocket Configuration (for WiFi Mode only)
const char* WEBSOCKET_SERVER_IP = "192.168.137.1";
const uint16_t WEBSOCKET_SERVER_PORT = 8080;

// Device Configuration
const char* DEVICE_ID = "ESP1_MAIN";

// ============ MODE DETECTION ============

enum OperationMode {
  MODE_USB,       // USB only - no WiFi
  MODE_WIFI       // WiFi + WebSocket
};

OperationMode currentMode;

// ============ VARIABLES ============

int receiveMessageCount = 0;
unsigned long lastStatusSend = 0;
const unsigned long STATUS_INTERVAL = 3000;

// WiFi Mode only
WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// =======================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=============================================");
  Serial.println("ESP1 - ESP-NOW Relay");
  Serial.println("=============================================\n");

  // Detect operation mode
  detectMode();
  
  // Mode-specific initialization
  if (currentMode == MODE_USB) {
    setupUSBMode();
  } else {
    setupWiFiMode();
  }
}

void loop() {
  if (currentMode == MODE_USB) {
    loopUSBMode();
  } else {
    loopWiFiMode();
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
    currentMode = MODE_USB;
    Serial.println("✓ USB MODE");
    Serial.println("ESP-NOW receive only → USB Serial");
    Serial.println("No WiFi (avoids channel conflicts)\n");
  } else {
    currentMode = MODE_WIFI;
    Serial.println("✓ WIFI MODE");
    Serial.println("ESP-NOW receive → WiFi WebSocket\n");
  }
}

// ============ ESP-NOW INITIALIZATION ============

void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }
  
  Serial.println("✓ ESP-NOW initialized");
  
  // Register receive callback
  esp_now_register_recv_cb(onESPNowDataReceived);
  
  uint8_t primary;
  wifi_second_chan_t secondary;
  esp_wifi_get_channel(&primary, &secondary);
  Serial.print("📡 Listening on channel: ");
  Serial.println(primary);
  Serial.println("Ready to receive ESP-NOW broadcasts\n");
}

// ============ USB MODE ============

void setupUSBMode() {
  Serial.println("--- USB Mode Setup ---");
  
  // Set WiFi to STA mode but don't connect (ESP-NOW only)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Force channel 1 for ESP-NOW
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  
  // Initialize ESP-NOW
  initESPNow();
  
  Serial.println("Ready! Receiving ESP-NOW → USB Serial\n");
}

void loopUSBMode() {
  // Send periodic status
  if (millis() - lastStatusSend > STATUS_INTERVAL) {
    lastStatusSend = millis();
    
    StaticJsonDocument<200> doc;
    doc["source"] = "USB";
    doc["device_id"] = DEVICE_ID;
    doc["mode"] = "USB";
    doc["received_count"] = receiveMessageCount;
    doc["uptime"] = millis() / 1000;
    doc["message"] = "ESP1 USB mode active";
    
    String json;
    serializeJson(doc, json);
    Serial.println(json);
  }
}

// ============ WIFI MODE ============

void setupWiFiMode() {
  Serial.println("--- WiFi Mode Setup ---");
  
  // Connect to WiFi
  initWiFi();
  
  // Initialize ESP-NOW (after WiFi to inherit channel)
  initESPNow();
  
  // Connect to WebSocket
  connectWebSocket();
  
  Serial.println("Ready! Receiving ESP-NOW → WiFi WebSocket\n");
}

void loopWiFiMode() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    initWiFi();
  }

  // Maintain WebSocket connection
  if (wsConnected) {
    wsClient.poll();
  } else {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      connectWebSocket();
    }
  }

  // Send periodic status
  if (millis() - lastStatusSend > STATUS_INTERVAL) {
    lastStatusSend = millis();
    
    if (wsConnected) {
      StaticJsonDocument<200> doc;
      doc["source"] = "WIFI";
      doc["device_id"] = DEVICE_ID;
      doc["mode"] = "WIFI";
      doc["received_count"] = receiveMessageCount;
      doc["rssi"] = WiFi.RSSI();
      doc["uptime"] = millis() / 1000;
      doc["message"] = "ESP1 WiFi mode active";
      
      String json;
      serializeJson(doc, json);
      wsClient.send(json);
    }
  }
}

// ============ WIFI INITIALIZATION ============

void initWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    uint8_t primary;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&primary, &secondary);
    Serial.print("Channel: ");
    Serial.println(primary);
    
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("\n❌ WiFi connection failed");
  }
}

void connectWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  String wsUrl = "ws://" + String(WEBSOCKET_SERVER_IP) + ":" + String(WEBSOCKET_SERVER_PORT);
  Serial.print("Connecting to WebSocket: ");
  Serial.println(wsUrl);

  wsConnected = wsClient.connect(wsUrl.c_str());

  if (wsConnected) {
    Serial.println("✓ WebSocket connected");
    
    StaticJsonDocument<200> doc;
    doc["source"] = "WIFI";
    doc["device_id"] = DEVICE_ID;
    doc["mode"] = "WIFI";
    doc["message"] = "ESP1 WiFi mode connected";
    doc["rssi"] = WiFi.RSSI();
    
    String json;
    serializeJson(doc, json);
    wsClient.send(json);
    
    wsClient.onEvent([](WebsocketsEvent event, String data) {
      if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WebSocket closed");
        wsConnected = false;
      }
    });
  } else {
    Serial.println("❌ WebSocket connection failed");
    wsConnected = false;
  }
}

// ============ ESP-NOW CALLBACK ============

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
  
  Serial.println("\n═══════════════════════════════════════════════════════");
  Serial.println("📡 ESP-NOW MESSAGE RECEIVED");
  Serial.println("═══════════════════════════════════════════════════════");
  Serial.print("From: ");
  Serial.println(macStr);
  Serial.print("Data: ");
  Serial.println(receivedData);
  Serial.print("Count: ");
  Serial.println(receiveMessageCount);
  Serial.println("═══════════════════════════════════════════════════════\n");
  
  // Forward based on mode
  if (currentMode == MODE_USB) {
    // USB mode - forward via Serial
    StaticJsonDocument<500> doc;
    doc["source"] = "USB";
    doc["device_id"] = DEVICE_ID;
    doc["mode"] = "USB";
    doc["sender_mac"] = macStr;
    doc["received_data"] = receivedData;
    doc["data_length"] = data_len;
    doc["receive_count"] = receiveMessageCount;
    doc["message"] = "ESP-NOW message received";
    
    String json;
    serializeJson(doc, json);
    Serial.println(json);
  } else {
    // WiFi mode - forward via WebSocket
    if (wsConnected) {
      StaticJsonDocument<400> doc;
      doc["source"] = "RELAY";
      doc["device_id"] = DEVICE_ID;
      doc["mode"] = "WIFI";
      doc["sender_mac"] = macStr;
      doc["relayed_data"] = receivedData;
      doc["rssi"] = WiFi.RSSI();
      doc["message"] = "ESP-NOW relay from " + String(macStr);
      
      String json;
      serializeJson(doc, json);
      wsClient.send(json);
      Serial.println("Relayed to Electron");
    }
  }
}
