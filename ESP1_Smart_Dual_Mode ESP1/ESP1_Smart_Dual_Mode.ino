/*
 * ESP1 - ESP-NOW TO MONITOR RELAY
 * 
 * ALWAYS DUAL TRANSPORT:
 * - Receives ESP-NOW messages
 * - Forwards via USB Serial (always)
 * - Forwards via WiFi WebSocket (always)
 * - Both transports active simultaneously
 * 
 * Hardware: Any ESP32 board
 * 
 * Setup:
 * 1. Install libraries:
 *    - ESP32 Board Support
 *    - ArduinoWebsockets (by Gil Maimon)
 *    - ArduinoJson (by Benoit Blanchon)
 * 2. Update WiFi credentials below
 * 3. Update WebSocket server IP below
 * 4. Upload to ESP32
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// ============ CONFIGURATION ============

// WiFi Configuration
const char* WIFI_SSID = "YOUR_HOTSPOT_NAME";
const char* WIFI_PASSWORD = "YOUR_HOTSPOT_PASSWORD";

// WebSocket Configuration
const char* WEBSOCKET_SERVER_IP = "192.168.137.1";
const uint16_t WEBSOCKET_SERVER_PORT = 8080;

// Device Configuration
const char* DEVICE_ID = "ESP1_MAIN";

// ============ TEST MODE CONFIGURATION ============
// Change this to test different modes:
// "USB_ONLY"  - Only USB Serial, no WiFi/WebSocket
// "WIFI_ONLY" - Only WiFi/WebSocket, no USB Serial  
// "DUAL"      - Both USB Serial AND WiFi/WebSocket
const char* TEST_MODE = "DUAL";  // <-- CHANGE THIS TO TEST

// ============ VARIABLES ============

int receiveMessageCount = 0;
unsigned long lastStatusSend = 0;
const unsigned long STATUS_INTERVAL = 5000;  // 5 seconds

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
  Serial.print("MODE: ");
  Serial.println(TEST_MODE);
  Serial.println("=============================================\n");

  // Initialize based on test mode
  if (strcmp(TEST_MODE, "USB_ONLY") == 0) {
    Serial.println("⚙️ USB ONLY MODE - No WiFi");
    // Only initialize ESP-NOW
    initESPNow();
    Serial.println("Ready! ESP-NOW → USB Serial ONLY\n");
  } 
  else if (strcmp(TEST_MODE, "WIFI_ONLY") == 0) {
    Serial.println("⚙️ WIFI ONLY MODE - No USB Serial forwarding");
    // Initialize WiFi and WebSocket
    initWiFi();
    initESPNow();
    connectWebSocket();
    Serial.println("Ready! ESP-NOW → WiFi WebSocket ONLY\n");
  }
  else { // DUAL mode
    Serial.println("⚙️ DUAL MODE - USB + WiFi");
    // Initialize WiFi and WebSocket
    initWiFi();
    initESPNow();
    connectWebSocket();
    Serial.println("Ready! ESP-NOW → USB Serial AND WiFi WebSocket\n");
  }
}

void loop() {
  // Maintain WiFi connection (skip in USB_ONLY mode)
  if (strcmp(TEST_MODE, "USB_ONLY") != 0) {
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
  }

  // Send periodic status based on mode
  if (millis() - lastStatusSend > STATUS_INTERVAL) {
    lastStatusSend = millis();
    
    // Via USB Serial (skip in WIFI_ONLY mode)
    if (strcmp(TEST_MODE, "WIFI_ONLY") != 0) {
      StaticJsonDocument<200> usbDoc;
      usbDoc["source"] = "USB";
      usbDoc["device_id"] = DEVICE_ID;
      usbDoc["mode"] = TEST_MODE;
      usbDoc["received_count"] = receiveMessageCount;
      usbDoc["uptime"] = millis() / 1000;
      usbDoc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
      usbDoc["ws_connected"] = wsConnected;
      usbDoc["message"] = String("ESP1 ") + TEST_MODE + " mode active";
      
      String usbJson;
      serializeJson(usbDoc, usbJson);
      Serial.println(usbJson);
    }
    
    // Via WebSocket (skip in USB_ONLY mode)
    if (strcmp(TEST_MODE, "USB_ONLY") != 0 && wsConnected) {
      StaticJsonDocument<200> wifiDoc;
      wifiDoc["source"] = "WIFI";
      wifiDoc["device_id"] = DEVICE_ID;
      wifiDoc["mode"] = TEST_MODE;
      wifiDoc["received_count"] = receiveMessageCount;
      wifiDoc["rssi"] = WiFi.RSSI();
      wifiDoc["uptime"] = millis() / 1000;
      wifiDoc["message"] = String("ESP1 ") + TEST_MODE + " mode active";
      
      String wifiJson;
      serializeJson(wifiDoc, wifiJson);
      wsClient.send(wifiJson);
    }
  }
  
  delay(10);
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

// ============ WIFI INITIALIZATION ============

void initWiFi() {
  Serial.println("Initializing WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  // In USB_ONLY mode, use STA mode only (no WiFi connection)
  // In WIFI modes, use AP+STA mode to receive ESP-NOW while connected to WiFi
  if (strcmp(TEST_MODE, "USB_ONLY") == 0) {
    WiFi.mode(WIFI_STA);
    Serial.println("Mode: WIFI_STA (USB_ONLY - no WiFi connection)");
  } else {
    WiFi.mode(WIFI_AP_STA);
    Serial.println("Mode: WIFI_AP_STA (for ESP-NOW + WiFi)");
  }
  
  Serial.print("STA MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("AP MAC Address (ESP-NOW): ");
  Serial.println(WiFi.softAPmacAddress());
  
  Serial.println("Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
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
  } else {
    Serial.println("\n❌ WiFi FAILED - Check SSID/password");
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
  // Immediate processing - no delay
  delay(100);  // Small delay to ensure stability
  
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
  Serial.println("═══════════════════════════════════════════════════════");
  
  delay(50);  // Brief pause before forwarding
  
  Serial.println("\n🔄 FORWARDING ESP-NOW MESSAGE TO MONITOR...");
  Serial.print("Mode: ");
  Serial.println(TEST_MODE);
  
  // Forward via USB Serial (skip in WIFI_ONLY mode)
  if (strcmp(TEST_MODE, "WIFI_ONLY") != 0) {
    // Forward the original ESP2 message directly without wrapping
    Serial.println("📤 USB: " + receivedData);
    Serial.flush();  // Ensure USB data is sent
    
    delay(100);  // Delay between USB and WiFi sends
  } else {
    Serial.println("⏭️ Skipping USB (WIFI_ONLY mode)");
  }
  
  // Forward via WebSocket (skip in USB_ONLY mode)
  if (strcmp(TEST_MODE, "USB_ONLY") != 0) {
    if (wsConnected) {
      // Forward the original ESP2 message directly without wrapping
      bool sent = wsClient.send(receivedData);
      Serial.print("📤 WiFi: ");
      Serial.println(sent ? "SUCCESS" : "FAILED");
    } else {
      Serial.println("⚠️ WiFi not connected - cannot send via WiFi");
    }
  } else {
    Serial.println("⏭️ Skipping WiFi (USB_ONLY mode)");
  }
  
  // Summary
  if (strcmp(TEST_MODE, "DUAL") == 0) {
    if (wsConnected) {
      Serial.println("✅ Sent via BOTH USB and WiFi");
    } else {
      Serial.println("⚠️ Sent via USB only (WiFi not connected)");
    }
  } else if (strcmp(TEST_MODE, "USB_ONLY") == 0) {
    Serial.println("✅ Sent via USB only");
  } else if (strcmp(TEST_MODE, "WIFI_ONLY") == 0) {
    if (wsConnected) {
      Serial.println("✅ Sent via WiFi only");
    } else {
      Serial.println("❌ WiFi not connected - message NOT forwarded!");
    }
  }
  
  delay(50);  // Final stability delay
  Serial.println("═══════════════════════════════════════════════════════\n");
}
