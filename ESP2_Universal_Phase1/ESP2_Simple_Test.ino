/*
 * ESP2 Simple Test - Minimal ESP-NOW Communication
 * 
 * This is a stripped-down version based on the working legacy code
 * to isolate and fix the ESP-NOW initialization issue.
 * 
 * Based on the working ESP2_WiFi_Relay.ino legacy code
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// Simple configuration
const char* DEVICE_ID = "ESP2_SIMPLE_TEST";
const int ESP_NOW_CHANNEL = 1;  // Fixed channel like legacy code

// Message counter
int messageCount = 0;

// Timing
unsigned long lastPing = 0;
const unsigned long PING_INTERVAL = 5000;  // Send ping every 5 seconds

// Function declarations
void initESPNow();
void sendSimplePing();
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP2 Simple Test - ESP-NOW Only");
  Serial.println("=================================");
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("=================================\n");

  // Initialize ESP-NOW using the exact same method as working legacy code
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.print("✓ WiFi mode set to STA\n");
  
  // Set the specific ESP-NOW channel (like legacy code)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  Serial.printf("✓ Channel set to: %d\n", ESP_NOW_CHANNEL);
  
  // Verify channel setting
  uint8_t primary;
  wifi_second_chan_t secondary;
  esp_wifi_get_channel(&primary, &secondary);
  Serial.printf("✓ Confirmed channel: %d\n", primary);
  
  // Initialize ESP-NOW
  initESPNow();
  
  Serial.println("\nReady to send pings to ESP1!\n");
}

void loop() {
  // Send periodic pings
  if (millis() - lastPing > PING_INTERVAL) {
    lastPing = millis();
    sendSimplePing();
  }
  
  delay(100);
}

// Initialize ESP-NOW (exact copy of working legacy code method)
void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  
  esp_err_t result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("❌ Error initializing ESP-NOW: %d\n", result);
    Serial.printf("❌ Error code meaning: %s\n", esp_err_to_name(result));
    return;
  }
  Serial.println("✓ ESP-NOW initialized successfully");
  
  // Register send callback (like legacy code)
  esp_now_register_send_cb(onDataSent);
  Serial.println("✓ Send callback registered");
  
  // Add broadcast peer (like legacy code)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
  peerInfo.channel = ESP_NOW_CHANNEL;
  peerInfo.encrypt = false;
  
  esp_err_t addPeerResult = esp_now_add_peer(&peerInfo);
  if (addPeerResult != ESP_OK) {
    Serial.printf("❌ Failed to add broadcast peer: %d\n", addPeerResult);
    Serial.printf("❌ Error code meaning: %s\n", esp_err_to_name(addPeerResult));
    return;
  }
  Serial.println("✓ Broadcast peer added successfully");
  Serial.printf("✓ Broadcasting on channel %d (ESP1 Gateway will receive)\n", ESP_NOW_CHANNEL);
}

// Send callback (like legacy code)
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("ESP-NOW Send to %s: %s\n", macStr, 
                status == ESP_NOW_SEND_SUCCESS ? "✓ SUCCESS" : "❌ FAILED");
}

// Send simple ping (like legacy code but simpler)
void sendSimplePing() {
  messageCount++;
  
  // Create simple JSON message (like legacy but minimal)
  String message = "{";
  message += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  message += "\"message_count\":" + String(messageCount) + ",";
  message += "\"uptime\":" + String(millis() / 1000) + ",";
  message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  message += "\"message\":\"Simple test ping from ESP2 to ESP1\"";
  message += "}";
  
  Serial.println("\n--- Sending Simple Ping ---");
  Serial.printf("Message #%d\n", messageCount);
  Serial.printf("Data: %s\n", message.c_str());
  
  // Broadcast to ESP1 (like legacy code)
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length());
  
  if (result == ESP_OK) {
    Serial.println("✓ ESP-NOW broadcast sent successfully");
  } else {
    Serial.printf("❌ Error sending ESP-NOW broadcast: %d (%s)\n", result, esp_err_to_name(result));
  }
  
  Serial.printf("Free heap after send: %d bytes\n", ESP.getFreeHeap());
}