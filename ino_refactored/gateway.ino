/*
 * Gateway - Central Receiver/Aggregator
 * 
 * Communication Flow:
 * 7. Receives GATEWAY_RELAY from towers
 *    Processes aggregated peer reports
 *    Outputs to USB Serial for monitor application
 * 
 * Receive-only - no constant pinging
 * 
 * Hardware: ESP32 connected via USB to PC/laptop
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// ============================================================================
//                         GATEWAY CONFIGURATION
// ============================================================================
const char* GATEWAY_ID = "GATEWAY_001";
const char* GATEWAY_NAME = "Central_Gateway";
const char* SHARED_KEY = "ndrrmc_441";
const int ESP_NOW_CHANNEL = 1;

// ============================================================================
//                         STATISTICS
// ============================================================================
int relaysReceived = 0;
int invalidMessages = 0;

// ============================================================================
//                         ESP-NOW RECEIVE CALLBACK
// ============================================================================
void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Get sender MAC
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  
  // Convert data to string
  String receivedData = "";
  for (int i = 0; i < len; i++) {
    receivedData += (char)data[i];
  }
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, receivedData);
  
  if (error) {
    invalidMessages++;
    Serial.printf("❌ Invalid JSON from %s\n", macStr);
    return;
  }
  
  // Validate shared key
  String key = doc["shared_key"] | "";
  if (key != SHARED_KEY) {
    invalidMessages++;
    Serial.printf("❌ Invalid key from %s\n", macStr);
    return;
  }
  
  String msgType = doc["type"] | "";
  
  if (msgType == "GATEWAY_RELAY") {
    handleGatewayRelay(doc, macStr, rssi);
  } else {
    // Log other message types for debugging
    Serial.printf("ℹ️ Received %s from %s (not processing)\n", msgType.c_str(), macStr);
  }
}

// ============================================================================
//                    7. HANDLE GATEWAY RELAY
// ============================================================================
void handleGatewayRelay(JsonDocument& doc, const char* senderMac, int rssi) {
  relaysReceived++;
  
  String relayTower = doc["relay_tower"] | "unknown";
  
  // Extract payload
  JsonObject payload = doc["payload"];
  String owner = payload["owner"] | "unknown";
  String messageId = payload["message_id"] | "";
  
  Serial.println("\n════════════════════════════════════════════════════════════");
  Serial.println("📥 GATEWAY_RELAY Received");
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.printf("   Relay Tower: %s\n", relayTower.c_str());
  Serial.printf("   Sender MAC: %s (RSSI: %d)\n", senderMac, rssi);
  Serial.printf("   Peer Owner: %s\n", owner.c_str());
  Serial.printf("   Message ID: %s\n", messageId.c_str());
  
  // Print tower distances
  JsonArray towers = payload["towers"];
  Serial.printf("   Towers (%d):\n", towers.size());
  for (JsonObject tower : towers) {
    String name = tower["name"] | "?";
    float dist = tower["distance"] | 0.0;
    Serial.printf("      • %s: %.2fm\n", name.c_str(), dist);
  }
  Serial.println("════════════════════════════════════════════════════════════");
  
  // Create clean output for monitor application
  JsonDocument output;
  output["gateway"] = GATEWAY_ID;
  output["timestamp"] = millis();
  output["relay_number"] = relaysReceived;
  output["relay_tower"] = relayTower;
  output["sender_mac"] = senderMac;
  output["rssi"] = rssi;
  
  // Peer data from payload
  output["owner"] = owner;
  output["message_id"] = messageId;
  
  // Copy towers
  JsonArray outTowers = output["towers"].to<JsonArray>();
  for (JsonObject srcTower : towers) {
    JsonObject dstTower = outTowers.add<JsonObject>();
    dstTower["name"] = srcTower["name"];
    dstTower["distance"] = srcTower["distance"];
  }
  
  // Output JSON for monitor to parse
  String outputJson;
  serializeJson(output, outputJson);
  Serial.println(outputJson);
  Serial.println("════════════════════════════════════════════════════════════\n");
}

// ============================================================================
//                         ESP-NOW INITIALIZATION
// ============================================================================
void initESPNow() {
  Serial.println("Initializing ESP-NOW...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    ESP.restart();
    return;
  }
  
  Serial.println("✓ ESP-NOW initialized");
  
  // Register receive callback only
  esp_now_register_recv_cb(onDataReceived);
  
  Serial.printf("✓ Listening on channel %d\n", ESP_NOW_CHANNEL);
}

// ============================================================================
//                              SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("    GATEWAY - Central Receiver");
  Serial.println("════════════════════════════════════════");
  Serial.printf("Gateway ID: %s\n", GATEWAY_ID);
  Serial.printf("Gateway Name: %s\n", GATEWAY_NAME);
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
  Serial.println("════════════════════════════════════════");
  Serial.println("Flow:");
  Serial.println("  7. Receive GATEWAY_RELAY from towers");
  Serial.println("     Process aggregated peer reports");
  Serial.println("     Output to USB Serial (monitor)");
  Serial.println("════════════════════════════════════════\n");
  
  initESPNow();
  
  Serial.println("🚀 Gateway Ready - Listening for relays...\n");
}

// ============================================================================
//                              MAIN LOOP
// ============================================================================
void loop() {
  // Gateway is receive-only, all work happens in callback
  delay(100);
}
