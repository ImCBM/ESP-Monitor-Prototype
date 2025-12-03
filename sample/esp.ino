#include <esp_now.h>
#include <WiFi.h>

#define BUTTON_PIN 12
#define MAX_PEERS 10

// -------------------------
// CHANGE ONLY THESE PER DEVICE
// -------------------------
const char* myID = "Mikoyan";        // Change to "Mikoyan" on the other device
const char* passphrase = "SECRET123"; // Must be identical on all devices

// -------------------------
// Message structure
// -------------------------
typedef struct message_t {
  char id[32];
  char text[128];
  char pass[32];
} message_t;

message_t outgoingMsg;
message_t incomingMsg;

String peersReceived[MAX_PEERS];
int peersReceivedCount = 0;

// -------------------------
// Receive callback
// -------------------------
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(message_t)) return; // sanity check
  memcpy(&incomingMsg, data, sizeof(incomingMsg));

  // Check passphrase
  if (strcmp(incomingMsg.pass, passphrase) != 0) return;

  // Track unique IDs (for both pings and feedback)
  bool exists = false;
  for (int i = 0; i < peersReceivedCount; i++) {
    if (peersReceived[i] == incomingMsg.id) {
      exists = true;
      break;
    }
  }
  if (!exists && peersReceivedCount < MAX_PEERS) {
    peersReceived[peersReceivedCount++] = String(incomingMsg.id);
  }

  Serial.printf("[RECEIVED] From %s: %s\n", incomingMsg.id, incomingMsg.text);

  // If this is a ping, send feedback
  if (strstr(incomingMsg.text, "Ping") != nullptr && strcmp(incomingMsg.id, myID) != 0) {
    snprintf(outgoingMsg.id, sizeof(outgoingMsg.id), "%s", myID);
    snprintf(outgoingMsg.text, sizeof(outgoingMsg.text), "Received feedback from %s", myID);
    snprintf(outgoingMsg.pass, sizeof(outgoingMsg.pass), "%s", passphrase);
    esp_now_send(info->src_addr, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));
  }
}

// -------------------------
// Send callback
// -------------------------
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Delivery Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// -------------------------
// Setup
// -------------------------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA); // Must be STA mode for ESP-NOW

  Serial.printf("Device %s ready!\n", myID);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSent);

  // Add broadcast peer
  uint8_t broadcastMAC[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMAC, 6);
  peerInfo.channel = 0;   // current channel
  peerInfo.encrypt = 0;   // no encryption for broadcast
  if(esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add broadcast peer");
  }
}

// -------------------------
// Loop
// -------------------------
void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Clear received list for this round
    peersReceivedCount = 0;

    // Prepare outgoing message
    snprintf(outgoingMsg.id, sizeof(outgoingMsg.id), "%s", myID);
    snprintf(outgoingMsg.text, sizeof(outgoingMsg.text), "Ping from %s", myID);
    snprintf(outgoingMsg.pass, sizeof(outgoingMsg.pass), "%s", passphrase);

    // Broadcast ping
    uint8_t broadcastMAC[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_err_t result = esp_now_send(broadcastMAC, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));

    if(result == ESP_OK) Serial.println("Sent Ping to all peers (broadcast)");
    else Serial.println("Failed to send broadcast");

    // Wait a short time for feedback (callbacks handle actual reception)
    unsigned long startTime = millis();
    while(millis() - startTime < 500){ // 500 ms for replies
      delay(10);
    }

    // Show received peers
    Serial.print("Received feedback from peers: ");
    if(peersReceivedCount == 0) Serial.println("None");
    else{
      for(int i=0; i<peersReceivedCount; i++){
        Serial.print(peersReceived[i]);
        if(i < peersReceivedCount-1) Serial.print(", ");
      }
      Serial.println();
    }

    delay(500); // debounce
  }
}
