/*
 * ESP2 - Sensor/Data ESP32 (No WiFi - ESP-NOW Only)
 * 
 * This ESP32 board sends data TO ESP1 MAIN via ESP-NOW:
 * 1. Sends periodic test/sensor data via ESP-NOW broadcast
 * 2. ESP1 (the MAIN board) receives and relays to Electron
 * 
 * This board does NOT connect to WiFi - it only uses ESP-NOW to communicate
 * with ESP1, which handles the WiFi/USB relay to Electron.
 * 
 * Hardware: Any ESP32 board
 * 
 * Setup Instructions:
 * 1. Install required libraries:
 *    - ESP32 Board Support (via Board Manager)
 *    - ArduinoJson (by Benoit Blanchon) - via Library Manager
 * 2. Upload to ESP32
 * 3. Power it on (battery, power supply, etc.)
 * 
 *  * Home WiFi → use 192.168.1.4
 * 
 * Laptop hotspot → use 192.168.137.
 */

#include <WiFi.h> 
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>

using namespace websockets;

// ============ IMPORTANT: WiFi SSID for Channel Detection ============
// ESP2 scans for this SSID to determine which WiFi channel ESP1 is using
// MUST match the WiFi network that ESP1 connects to!
const char* WIFI_SSID = "YOUR_HOTSPOT_NAME";  // <-- CHANGE THIS to match ESP1's WiFi
const char* WIFI_PASSWORD = "YOUR_HOTSPOT_PASSWORD";  // <-- CHANGE THIS to match ESP1's WiFi

// WebSocket Configuration (for direct WiFi mode)
const char* WEBSOCKET_SERVER_IP = "192.168.137.1";
const uint16_t WEBSOCKET_SERVER_PORT = 8080;

// Device Configuration
const char* DEVICE_ID = "ESP2_SENSOR";

// Data sending interval
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 5000; // Send data every 5 seconds

// Mode switching configuration
unsigned long lastModeSwitch = 0;
const unsigned long MODE_SWITCH_INTERVAL = 30000; // Switch every 30 seconds
bool useESPNOW = true; // Start with ESP-NOW mode
bool wifiConnected = false;

// Message counter
int messageCount = 0;

// Store detected WiFi channel
int32_t detectedChannel = 1;

// WebSocket client for direct WiFi communication
WebsocketsClient wsClient;
bool wsConnected = false;

// Function declarations
void initESPNow();
void sendDataViaESPNOW();
void sendDataViaWiFi();
void connectToWiFi();
void connectWebSocket();
int32_t getWiFiChannel(const char *ssid);
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP2 - Sensor/Data ESP32");
  Serial.println("=================================\n");

  // Set device as WiFi station (required for ESP-NOW, but not connecting to WiFi)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Auto-detect WiFi channel by scanning for ESP1's network
  detectedChannel = getWiFiChannel(WIFI_SSID);
  
  if (detectedChannel > 0) {
    Serial.print("✓ Detected WiFi channel: ");
    Serial.println(detectedChannel);
    
    // Set ESP2 to use the same channel as ESP1
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(detectedChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    
    Serial.print("✓ ESP2 channel set to: ");
    uint8_t primary;
    wifi_second_chan_t secondary;
    esp_wifi_get_channel(&primary, &secondary);
    Serial.println(primary);
  } else {
    Serial.println("⚠️ WARNING: Could not detect WiFi channel!");
    Serial.println("⚠️ Make sure WIFI_SSID matches ESP1's network");
    Serial.println("⚠️ Using default channel 1 - ESP-NOW may not work!");
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  }
  
  // Initialize ESP-NOW for first mode
  initESPNow();
  
  Serial.println("\nReady! This board will alternate every 30 seconds:");
  Serial.println("  Mode 1 (30s): ESP-NOW → ESP1 → Monitor");
  Serial.println("  Mode 2 (30s): WiFi Direct → Monitor");
  Serial.println("  Starting with ESP-NOW mode...\n");
}

void loop() {
  // Check if it's time to switch modes
  if (millis() - lastModeSwitch > MODE_SWITCH_INTERVAL) {
    lastModeSwitch = millis();
    useESPNOW = !useESPNOW; // Toggle mode
    
    if (useESPNOW) {
      Serial.println("\n🔄 SWITCHING TO ESP-NOW MODE");
      Serial.println("═══════════════════════════════════");
      // Disconnect WiFi and WebSocket
      if (wsConnected) {
        wsClient.close();
        wsConnected = false;
      }
      WiFi.disconnect();
      wifiConnected = false;
      
      // Reinitialize for ESP-NOW
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      
      // Re-detect channel and reinitialize ESP-NOW
      detectedChannel = getWiFiChannel(WIFI_SSID);
      if (detectedChannel > 0) {
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(detectedChannel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
      } else {
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      }
      
      // Reinitialize ESP-NOW
      initESPNow();
      
      Serial.println("✓ ESP-NOW mode active - messages go via ESP1");
      Serial.println("═══════════════════════════════════\n");
    } else {
      Serial.println("\n🔄 SWITCHING TO WIFI DIRECT MODE");
      Serial.println("═══════════════════════════════════");
      // Deinitialize ESP-NOW
      esp_now_deinit();
      
      // Connect to WiFi
      connectToWiFi();
      
      Serial.println("✓ WiFi Direct mode active - messages go directly to monitor");
      Serial.println("═══════════════════════════════════\n");
    }
  }

  // Send periodic data based on current mode
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    
    if (useESPNOW) {
      sendDataViaESPNOW();
    } else {
      sendDataViaWiFi();
    }
  }

  // Maintain WebSocket connection in WiFi mode
  if (!useESPNOW && wsConnected) {
    wsClient.poll();
  }

  delay(10);
}


// ============ WiFi Channel Detection Function ============
// Scans for the specified SSID and returns its WiFi channel
// This ensures ESP2 uses the same channel as ESP1's WiFi network
int32_t getWiFiChannel(const char *ssid) {
  Serial.print("Scanning for WiFi network: ");
  Serial.println(ssid);
  
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("⚠️ No WiFi networks found!");
    return 0;
  }
  
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks:");
  
  for (int i = 0; i < n; i++) {
    Serial.print("  - ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (Channel ");
    Serial.print(WiFi.channel(i));
    Serial.println(")");
    
    if (strcmp(ssid, WiFi.SSID(i).c_str()) == 0) {
      Serial.print("✓ Match found! Channel: ");
      Serial.println(WiFi.channel(i));
      return WiFi.channel(i);
    }
  }
  
  Serial.println("⚠️ Target network not found!");
  return 0;
}

// Initialize ESP-NOW
void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW initialized successfully");
  
  // Register send callback
  esp_now_register_send_cb(onDataSent);
  
  // Add broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
  peerInfo.channel = detectedChannel;  // Use detected channel, not hardcoded 1
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
    return;
  }
  Serial.println("Broadcast peer added successfully");
}

// Callback when data is sent (new API)
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
           tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
  Serial.print("ESP-NOW Send Status to ");
  Serial.print(macStr);
  Serial.print(": ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Send test/sensor data to ESP1 via ESP-NOW broadcast
void sendDataViaESPNOW() {
  messageCount++;
  String message = "{";
  message += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  message += "\"message_count\":" + String(messageCount) + ",";
  message += "\"uptime\":" + String(millis() / 1000) + ",";
  message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  message += "\"communication_mode\":\"ESP-NOW\",";
  message += "\"message\":\"HELLO THIS IS ESP2, sending data from ESP2 to ESP1 MAIN using ESP-NOW\"";
  message += "}";
  
  Serial.println("\n--- ESP-NOW Mode: Sending Data via ESP1 ---");
  Serial.println("Message #" + String(messageCount));
  Serial.println("Data: " + message);
  
  // Broadcast to all ESPs (ESP1 will receive it)
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
  if (result == ESP_OK) {
    Serial.println("✓ ESP-NOW broadcast sent - ESP1 will relay to monitor");
  } else {
    Serial.println("❌ Error sending ESP-NOW broadcast");
  }
}

// Send data directly to monitor via WiFi WebSocket
void sendDataViaWiFi() {
  messageCount++;
  String message = "{";
  message += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  message += "\"message_count\":" + String(messageCount) + ",";
  message += "\"uptime\":" + String(millis() / 1000) + ",";
  message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  message += "\"communication_mode\":\"WiFi-Direct\",";
  message += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  message += "\"message\":\"HELLO THIS IS ESP2, sending data from ESP2 directly to Monitor using WiFi\"";
  message += "}";
  
  Serial.println("\n--- WiFi Direct Mode: Sending Data Directly ---");
  Serial.println("Message #" + String(messageCount));
  Serial.println("Data: " + message);
  
  if (wsConnected) {
    bool sent = wsClient.send(message);
    if (sent) {
      Serial.println("✓ WiFi Direct message sent to monitor");
    } else {
      Serial.println("❌ Failed to send WiFi Direct message");
    }
  } else {
    Serial.println("❌ WebSocket not connected - attempting reconnect...");
    connectWebSocket();
  }
}

// Connect to WiFi network
void connectToWiFi() {
  Serial.println("Connecting to WiFi for direct communication...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✓ WiFi connected for direct communication!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Connect to WebSocket
    connectWebSocket();
  } else {
    wifiConnected = false;
    Serial.println("\n❌ WiFi connection failed!");
  }
}

// Connect to WebSocket server
void connectWebSocket() {
  if (!wifiConnected) {
    return;
  }
  
  String wsUrl = "ws://" + String(WEBSOCKET_SERVER_IP) + ":" + String(WEBSOCKET_SERVER_PORT);
  Serial.print("Connecting to WebSocket: ");
  Serial.println(wsUrl);
  
  wsConnected = wsClient.connect(wsUrl.c_str());
  
  if (wsConnected) {
    Serial.println("✓ WebSocket connected for direct communication");
    
    // Send initial connection message
    String connectMsg = "{";
    connectMsg += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
    connectMsg += "\"communication_mode\":\"WiFi-Direct\",";
    connectMsg += "\"message\":\"ESP2 WiFi Direct mode connected\",";
    connectMsg += "\"rssi\":" + String(WiFi.RSSI());
    connectMsg += "}";
    
    wsClient.send(connectMsg);
    
    wsClient.onEvent([](WebsocketsEvent event, String data) {
      if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WebSocket connection closed");
        wsConnected = false;
      }
    });
  } else {
    Serial.println("❌ WebSocket connection failed");
    wsConnected = false;
  }
}
