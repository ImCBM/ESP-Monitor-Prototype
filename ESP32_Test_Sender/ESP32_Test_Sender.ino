/*
 * ESP32 Test Sender (No WiFi - ESP-NOW Only)
 * 
 * This is a test ESP32 that only uses ESP-NOW to send messages.
 * It has no WiFi connection and relies on other ESPs to relay its messages.
 * 
 * Test Scenario:
 * This ESP -> ESP-NOW -> WiFi+Relay ESP -> WebSocket -> Electron
 *          -> ESP-NOW -> USB ESPmain -> Serial -> Electron
 * 
 * Hardware: Any ESP32 board
 * 
 * Setup Instructions:
 * 1. Install ESP32 Board Support (via Board Manager)
 * 2. Update RECEIVER_MAC_ADDRESS to match your WiFi+Relay ESP's MAC
 * 3. Optionally update RECEIVER_MAC_ADDRESS_2 to match USB ESPmain MAC
 * 4. Upload to ESP32
 */

#include <esp_now.h>
#include <WiFi.h>

// Device Configuration
const char* DEVICE_ID = "ESP_TEST_SENDER";

// Receiver MAC addresses (update these to match your ESPs)
// To find MAC address: Upload a basic sketch that prints WiFi.macAddress()
uint8_t receiverMAC1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // WiFi+Relay ESP MAC
uint8_t receiverMAC2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // USB ESPmain MAC

// Peer info structures
esp_now_peer_info_t peerInfo1;
esp_now_peer_info_t peerInfo2;

// Message counter
int messageCount = 0;

// Data sending interval
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 5000; // Send data every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n====================================");
  Serial.println("ESP32 Test Sender (ESP-NOW Only)");
  Serial.println("====================================\n");

  // Set device as WiFi station (required for ESP-NOW, but not connecting to WiFi)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("\nIMPORTANT: Update the receiver MAC addresses in the code!");
  Serial.println("Receiver 1 (WiFi+Relay): " + macToString(receiverMAC1));
  Serial.println("Receiver 2 (USB ESPmain): " + macToString(receiverMAC2));

  // Initialize ESP-NOW
  initESPNow();
  
  // Register peers
  registerPeers();
  
  Serial.println("\nTest sender ready. Sending messages every 5 seconds...\n");
}

void loop() {
  // Send periodic test messages
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendTestMessage();
  }

  delay(10);
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
}

// Register peer devices
void registerPeers() {
  // Register peer 1 (WiFi+Relay ESP)
  memset(&peerInfo1, 0, sizeof(peerInfo1));
  memcpy(peerInfo1.peer_addr, receiverMAC1, 6);
  peerInfo1.channel = 0;
  peerInfo1.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo1) != ESP_OK) {
    Serial.println("Failed to add peer 1 (WiFi+Relay)");
  } else {
    Serial.println("Peer 1 (WiFi+Relay) added successfully");
  }
  
  // Register peer 2 (USB ESPmain)
  memset(&peerInfo2, 0, sizeof(peerInfo2));
  memcpy(peerInfo2.peer_addr, receiverMAC2, 6);
  peerInfo2.channel = 0;
  peerInfo2.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo2) != ESP_OK) {
    Serial.println("Failed to add peer 2 (USB ESPmain)");
  } else {
    Serial.println("Peer 2 (USB ESPmain) added successfully");
  }
}

// Callback when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send to ");
  Serial.print(macToString((uint8_t*)mac_addr));
  Serial.print(": ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Send test message via ESP-NOW
void sendTestMessage() {
  messageCount++;
  
  String message = "{";
  message += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  message += "\"message_count\":" + String(messageCount) + ",";
  message += "\"uptime\":" + String(millis() / 1000) + ",";
  message += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  message += "\"message\":\"Test message from ESP-NOW sender\"";
  message += "}";
  
  Serial.println("\n--- Sending Test Message ---");
  Serial.println("Message #" + String(messageCount));
  Serial.println("Data: " + message);
  
  // Send to both receivers
  esp_err_t result1 = esp_now_send(receiverMAC1, (uint8_t*)message.c_str(), message.length());
  esp_err_t result2 = esp_now_send(receiverMAC2, (uint8_t*)message.c_str(), message.length());
  
  if (result1 == ESP_OK) {
    Serial.println("Sent to peer 1 (WiFi+Relay)");
  } else {
    Serial.println("Error sending to peer 1");
  }
  
  if (result2 == ESP_OK) {
    Serial.println("Sent to peer 2 (USB ESPmain)");
  } else {
    Serial.println("Error sending to peer 2");
  }
}

// Convert MAC address to string
String macToString(uint8_t* mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}
