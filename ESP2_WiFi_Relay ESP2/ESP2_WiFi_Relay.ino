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

// ============ IMPORTANT: WiFi SSID for Channel Detection ============
// ESP2 scans for this SSID to determine which WiFi channel ESP1 is using
// MUST match the WiFi network that ESP1 connects to!
const char* WIFI_SSID = "YOUR_HOTSPOT_NAME";  // <-- CHANGE THIS to match ESP1's WiFi

// Device Configuration
const char* DEVICE_ID = "ESP2_SENSOR";

// Data sending interval
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 5000; // Send data every 5 seconds

// Message counter
int messageCount = 0;

// Store detected WiFi channel
int32_t detectedChannel = 1;

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
  
  // Initialize ESP-NOW
  initESPNow();
  
  Serial.println("\nReady! This board will:");
  Serial.println("  1. Send test/sensor data via ESP-NOW broadcast every 5 seconds");
  Serial.println("  2. ESP1 MAIN will receive and relay to Electron\n");
}

void loop() {
  // Send periodic test/sensor data via ESP-NOW
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendDataToESP1();
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
void sendDataToESP1() {
  messageCount++;
  String message = "{";
  message += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  message += "\"message_count\":" + String(messageCount) + ",";
  message += "\"uptime\":" + String(millis() / 1000) + ",";
  message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  message += "\"message\":\"HELLO THIS IS ESP2, sending data from ESP2 to ESP1 MAIN\"";
  message += "}";
  Serial.println("\n--- Sending Data to ESP1 via ESP-NOW ---");
  Serial.println("Message #" + String(messageCount));
  Serial.println("Data: " + message);
  // Broadcast to all ESPs (ESP1 will receive it)
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
  if (result == ESP_OK) {
    Serial.println("Broadcast sent - ESP1 MAIN will receive and relay");
  } else {
    Serial.println("Error sending broadcast");
  }
}
