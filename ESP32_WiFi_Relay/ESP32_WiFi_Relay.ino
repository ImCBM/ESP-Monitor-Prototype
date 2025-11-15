/*
 * ESP32 WiFi + Relay Board
 * 
 * This ESP32 board performs two functions:
 * 1. Connects to WiFi and sends data to Electron app via WebSocket
 * 2. Receives ESP-NOW messages from other ESPs and relays them to Electron
 * 
 * Hardware: Any ESP32 board with WiFi capability
 * 
 * Setup Instructions:
 * 1. Install required libraries:
 *    - ESP32 Board Support (via Board Manager)
 *    - ArduinoWebsockets (by Gil Maimon) - via Library Manager
 * 2. Update WiFi credentials (WIFI_SSID and WIFI_PASSWORD)
 * 3. Update WebSocket server IP (WEBSOCKET_SERVER_IP) to your computer's IP
 * 4. Upload to ESP32
 */

#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";          // Change this
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Change this

// WebSocket Configuration
const char* WEBSOCKET_SERVER_IP = "192.168.1.100"; // Change to your PC's IP
const uint16_t WEBSOCKET_SERVER_PORT = 8080;

// Device Configuration
const char* DEVICE_ID = "ESP_WIFI_RELAY";

WebsocketsClient wsClient;
bool wsConnected = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// Data sending interval
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 2000; // Send data every 2 seconds

// Message counter
int messageCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32 WiFi + Relay Board Starting");
  Serial.println("=================================\n");

  // Initialize WiFi
  initWiFi();
  
  // Initialize ESP-NOW
  initESPNow();
  
  // Connect to WebSocket
  connectWebSocket();
}

void loop() {
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
    sendWiFiData();
  }

  delay(10);
}

// Initialize WiFi
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
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }
}

// Initialize ESP-NOW
void initESPNow() {
  Serial.println("\nInitializing ESP-NOW...");
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  Serial.println("ESP-NOW initialized successfully");
  
  // Register callback for receiving data
  esp_now_register_recv_cb(onESPNowDataReceived);
}

// ESP-NOW receive callback
void onESPNowDataReceived(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  Serial.println("\n--- ESP-NOW Message Received ---");
  
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  Serial.print("From MAC: ");
  Serial.println(macStr);
  Serial.print("Data length: ");
  Serial.println(data_len);
  
  // Convert received data to string
  String receivedData = "";
  for (int i = 0; i < data_len; i++) {
    receivedData += (char)data[i];
  }
  
  Serial.print("Data: ");
  Serial.println(receivedData);
  
  // Relay to Electron via WebSocket
  relayToElectron(macStr, receivedData);
}

// Connect to WebSocket server
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
    doc["message"] = "WiFi ESP connected";
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

// Send WiFi data to Electron
void sendWiFiData() {
  if (!wsConnected) return;

  messageCount++;
  
  StaticJsonDocument<300> doc;
  doc["source"] = "WIFI";
  doc["device_id"] = DEVICE_ID;
  doc["message_count"] = messageCount;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["message"] = "Periodic status update from WiFi ESP";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  wsClient.send(jsonString);
  
  Serial.println("Sent WiFi data: " + jsonString);
}

// Relay ESP-NOW data to Electron via WebSocket
void relayToElectron(String senderMAC, String data) {
  if (!wsConnected) {
    Serial.println("WebSocket not connected. Cannot relay data.");
    return;
  }

  StaticJsonDocument<400> doc;
  doc["source"] = "RELAY";
  doc["device_id"] = DEVICE_ID;
  doc["sender_mac"] = senderMAC;
  doc["relayed_data"] = data;
  doc["rssi"] = WiFi.RSSI();
  doc["message"] = "Relayed message from ESP-NOW";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  wsClient.send(jsonString);
  
  Serial.println("Relayed to Electron: " + jsonString);
}
