/*
 * ESP32 USB ESPmain Board
 * 
 * This ESP32 board performs two functions:
 * 1. Receives ESP-NOW messages from WiFi+Relay ESP
 * 2. Sends all data to Electron app via USB Serial connection
 * 
 * This provides an offline/fallback communication path when WiFi is not available
 * or for redundancy.
 * 
 * Hardware: Any ESP32 board with USB capability
 * 
 * Setup Instructions:
 * 1. Install required libraries:
 *    - ESP32 Board Support (via Board Manager)
 *    - ArduinoJson (by Benoit Blanchon) - via Library Manager
 * 2. Upload to ESP32
 * 3. Connect to PC via USB
 * 4. Select the serial port in Electron app
 */

#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Device Configuration
const char* DEVICE_ID = "ESP_USB_MAIN";

// Message counter
int messageCount = 0;

// Data sending interval (for periodic updates)
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 3000; // Send status every 3 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("ESP32 USB ESPmain Board Starting");
  Serial.println("=================================\n");

  // Set device as WiFi station (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Initialize ESP-NOW
  initESPNow();
  
  // Send initial status
  sendStatus("USB ESPmain initialized and ready");
}

void loop() {
  // Send periodic status updates
  if (millis() - lastDataSend > DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendPeriodicStatus();
  }

  delay(10);
}

// Initialize ESP-NOW
void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    sendStatus("ERROR: Failed to initialize ESP-NOW");
    return;
  }
  
  Serial.println("ESP-NOW initialized successfully");
  
  // Register callback for receiving data
  esp_now_register_recv_cb(onESPNowDataReceived);
  
  sendStatus("ESP-NOW initialized successfully");
}

// ESP-NOW receive callback
void onESPNowDataReceived(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  messageCount++;
  
  // Get sender MAC address
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  // Convert received data to string
  String receivedData = "";
  for (int i = 0; i < data_len; i++) {
    receivedData += (char)data[i];
  }
  
  // Send to Electron via Serial
  sendReceivedData(macStr, receivedData, data_len);
}

// Send received ESP-NOW data to Electron via Serial
void sendReceivedData(String senderMAC, String data, int dataLength) {
  StaticJsonDocument<500> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["sender_mac"] = senderMAC;
  doc["received_data"] = data;
  doc["data_length"] = dataLength;
  doc["message_count"] = messageCount;
  doc["uptime"] = millis() / 1000;
  doc["message"] = "ESP-NOW message received via USB path";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println(jsonString);
}

// Send periodic status update
void sendPeriodicStatus() {
  StaticJsonDocument<300> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["message_count"] = messageCount;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["message"] = "Periodic status from USB ESPmain";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println(jsonString);
}

// Send status message to Electron via Serial
void sendStatus(String statusMessage) {
  StaticJsonDocument<200> doc;
  doc["source"] = "USB";
  doc["device_id"] = DEVICE_ID;
  doc["message"] = statusMessage;
  doc["uptime"] = millis() / 1000;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println(jsonString);
}
