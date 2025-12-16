#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 FULL CLEANUP START ---");

  // 1. ERASE WIFI CREDENTIALS
  Serial.println("Erasing WiFi settings...");
  WiFi.disconnect(true, true);  
  delay(1000);

  // 2. ERASE NVS (Non-volatile Storage)
  Serial.println("Erasing NVS flash...");
  esp_err_t err = nvs_flash_erase();  
  if (err == ESP_OK) {
    Serial.println("NVS erase OK!");
  } else {
    Serial.printf("NVS erase error: %d\n", err);
  }
  delay(1000);

  // 3. RE-INITIALIZE NVS
  Serial.println("Re-initializing NVS...");
  err = nvs_flash_init();
  if (err == ESP_OK) {
    Serial.println("NVS init OK!");
  } else {
    Serial.printf("NVS init error: %d\n", err);
  }
  delay(1000);

  Serial.println("--- CLEANUP COMPLETE ---");
}

void loop() {
  // Do nothing. Run once.
}
