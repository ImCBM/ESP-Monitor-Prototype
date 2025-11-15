# ESP-Monitor-Prototype - Quick Start Guide

## What You Have

✅ **Electron App** - Complete monitoring application
✅ **3 ESP32 Programs** - Ready to upload to your boards
✅ **Full Documentation** - Detailed README with setup instructions

## Next Steps

### 1. Install Electron Dependencies (5 minutes)

```cmd
npm install
```

### 2. Test the Electron App

```cmd
npm start
```

You should see the monitor interface with red status indicators (nothing connected yet).

### 3. Prepare Your ESP32 Boards

You need at least **2 ESP32 boards** for basic testing:
- Board 1: WiFi+Relay ESP
- Board 2: USB ESPmain
- (Optional) Board 3: Test Sender

### 4. Upload ESP32 Code

#### Board 1 - WiFi+Relay ESP (`ESP32_WiFi_Relay/`)
1. Open in Arduino IDE
2. **IMPORTANT**: Update these lines:
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_SSID";          // Your WiFi name
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Your WiFi password
   const char* WEBSOCKET_SERVER_IP = "192.168.1.100"; // Your PC's IP
   ```
3. Upload to first ESP32
4. Open Serial Monitor (115200 baud) and copy the MAC address

#### Board 2 - USB ESPmain (`ESP32_USB_Main/`)
1. Open in Arduino IDE
2. Upload to second ESP32
3. Keep connected via USB to your PC
4. Note the COM port (e.g., COM3)

#### Board 3 - Test Sender (`ESP32_Test_Sender/`) - Optional
1. Open in Arduino IDE
2. Update receiver MAC addresses with the MACs from Board 1 & 2:
   ```cpp
   uint8_t receiverMAC1[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX}; // Board 1 MAC
   uint8_t receiverMAC2[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX}; // Board 2 MAC
   ```
3. Upload to third ESP32

### 5. Connect Everything

1. **Start Electron app**: `npm start`
2. **Power WiFi+Relay ESP** - WiFi indicator should turn green
3. **In Electron**: Select serial port (COM3 or similar) and click "Connect"
4. **USB indicator** should turn green
5. **Power Test Sender** (optional) - You'll see relay messages

### 6. Watch the Magic Happen!

You should see:
- ✅ Green status indicators for connected devices
- 📊 Real-time log messages with color coding
- 📡 Signal strength (RSSI) for WiFi connection
- 🔄 Messages from all communication paths

## Testing Communication Paths

### WiFi Path
WiFi+Relay ESP automatically sends status every 2 seconds

### Relay Path  
Test Sender → ESP-NOW → WiFi+Relay ESP → WebSocket → Electron

### USB Path
Test Sender → ESP-NOW → USB ESPmain → Serial → Electron

## Common Issues

### Can't find your PC's IP address?
Windows Command Prompt:
```cmd
ipconfig
```
Look for "IPv4 Address" under your WiFi adapter

### ESP32 won't connect to WiFi?
- Make sure you're using 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Double-check SSID and password (case-sensitive)
- Try moving ESP32 closer to router

### Serial port not showing up?
- Install CH340 or CP2102 USB drivers
- Try a different USB cable (some are power-only)
- Check Device Manager for COM ports

### WebSocket connection fails?
- Check Windows Firewall settings
- Verify IP address is correct
- Make sure Electron app is running first

## Project Structure

```
ESP-Monitor-Prototype/
├── 📦 Electron App Files
│   ├── package.json
│   ├── main.js (WebSocket & Serial logic)
│   ├── preload.js (IPC bridge)
│   ├── index.html (UI structure)
│   ├── styles.css (Beautiful styling)
│   └── renderer.js (UI logic)
│
└── 🔧 ESP32 Programs
    ├── ESP32_WiFi_Relay/ (Board 1)
    ├── ESP32_USB_Main/ (Board 2)
    └── ESP32_Test_Sender/ (Board 3 - optional)
```

## Questions?

Check the full `README.md` for detailed documentation!
