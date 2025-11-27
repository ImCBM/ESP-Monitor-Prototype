# ESP2 Universal Firmware - Phase 1-3 Implementation

## Overview
This is the Phase 1-3 implementation of the universal ESP2 firmware, featuring **Core Communication & Envelope**, **WiFi/Server Scanning & Mode Switching**, and **Enhanced Peer Discovery & Handshake** functionality. All ESP2 devices run the same code with only device-specific configuration changes.

## Phase 1-3 Features

### ✅ Phase 1: Core Communication & Envelope
- **Standardized JSON Envelope Structure**: All messages use a consistent envelope format
- **ESP-NOW Peer-to-Peer Communication**: Direct device-to-device messaging
- **Basic Peer Discovery Protocol**: Broadcast ping mechanism to find nearby ESP2 devices
- **Message Authentication**: Shared key validation for security

### ✅ Phase 2: WiFi/Server Scanning & Mode Switching  
- **Periodic WiFi Network Scanning**: Automatic discovery of known and open networks
- **Server Reachability Checks**: Connectivity testing to monitor servers
- **Dynamic Mode Switching**: Intelligent switching between communication modes:
  - `ESP-NOW Only`: No WiFi available
  - `WiFi Backup`: WiFi available, ESP-NOW primary
  - `WiFi Primary`: WiFi + server available, WiFi primary
  - `WiFi Only`: WiFi exclusive mode
- **Known Network Management**: Pre-configured WiFi credentials list

### ✅ Phase 3: Enhanced Peer Discovery & Handshake
- **Advanced Peer Validation**: Enhanced security with device type and firmware validation
- **Improved Handshake Protocol**: Capability negotiation and trust establishment
- **Peer Capability Discovery**: Dynamic capability exchange and tracking
- **Enhanced Loop Prevention**: Sophisticated handshake attempt tracking
- **Trust Management**: Peer validation and trust status tracking

## Quick Setup

### 1. Hardware Requirements
- Any ESP32 development board
- No additional sensors required for Phase 1-3

### 2. Library Dependencies
Install these libraries via Arduino IDE Library Manager:
- **ESP32 Board Support** (via Board Manager)
- **ArduinoJson** by Benoit Blanchon

### 3. Device Configuration
Edit these lines in the code for each ESP2 device:

```cpp
// CHANGE THESE FOR EACH ESP2 DEVICE
const char* DEVICE_ID = "ESP2_SENSOR_001";        // Unique identifier
const char* DEVICE_OWNER = "user_alice";          // Owner name
const char* SHARED_KEY = "ESP2_NETWORK_KEY";      // Authentication key
```

### 4. WiFi Network Configuration
Update the known networks list:

```cpp
WiFiCredential knownNetworks[] = {
  {"YourHomeWiFi", "your_password", false},
  {"YourHotspot", "hotspot_password", false},
  {"OfficeWiFi", "office_password", false},
  {"OpenNetwork", "", true},
  {"", "", false} // End marker
};
```

### 5. Upload and Test
1. Upload the firmware to each ESP32
2. Open Serial Monitor (115200 baud)
3. Watch for enhanced peer discovery and WiFi scanning
4. Deploy multiple ESP2s to see advanced inter-device communication

## Communication Modes

The firmware automatically switches between four communication modes:

### Mode 1: ESP-NOW Only
- **When**: No WiFi networks available
- **Behavior**: Pure ESP-NOW peer-to-peer communication
- **Use Case**: Remote areas, backup communication

### Mode 2: WiFi Backup
- **When**: WiFi connected but server unreachable
- **Behavior**: ESP-NOW primary, WiFi as backup
- **Use Case**: Local networks without internet

### Mode 3: WiFi Primary
- **When**: WiFi connected and server reachable
- **Behavior**: WiFi primary for server communication, ESP-NOW for peer mesh
- **Use Case**: Normal operation with full connectivity

### Mode 4: WiFi Only
- **When**: Configured for WiFi-exclusive operation
- **Behavior**: No ESP-NOW, WiFi communication only
- **Use Case**: High-bandwidth applications

## Testing Phase 1-3

### Single Device Test
1. Upload firmware to one ESP32
2. Should see periodic ping broadcasts and WiFi scanning
3. Monitor mode switching based on network availability

### Multi-Device Test
1. Configure 2-3 ESP32s with different DEVICE_IDs
2. Upload same firmware to all devices
3. Power on all devices near a known WiFi network
4. Observe:
   - Enhanced peer discovery with capability exchange
   - WiFi network detection and connection
   - Mode switching based on connectivity
   - Trust establishment through handshakes

### Network Integration Test
1. Set up a WiFi hotspot matching your known networks
2. Deploy ESP32s in range
3. Watch mode transitions from ESP-NOW Only to WiFi Primary
4. Test server reachability detection

## Expected Serial Output

```
=================================
ESP2 Universal Firmware - Phase 1-3
=================================
Device ID: ESP2_SENSOR_001
Owner: user_alice
Device Type: ESP2_UNIVERSAL
Firmware Version: 2.0.0
Protocol Version: 2.0
=================================

🔍 Scanning for WiFi networks...
Found 3 networks:
  YourHotspot (RSSI: -45) [SECURED]
    ✓ Known network: YourHotspot
🔗 Attempting connection to: YourHotspot
✅ Connected to: YourHotspot
🌐 Checking server reachability...
✅ Server reachable: 8.8.8.8:53
🔄 Mode change: ESP-NOW Only -> WiFi Primary

📡 Broadcasting Enhanced Peer Discovery Ping
Mode: WiFi Primary | WiFi: Connected | Server: Reachable
✓ Enhanced ping broadcast sent

📥 ESP-NOW Message Received
From: BB:CC:DD:EE:FF:AA
✓ Envelope validation passed
Processing ping message from ESP2_SENSOR_002 (user_bob) [ESP2_UNIVERSAL v2.0.0]
  Peer status: Mode=2, WiFi=Yes, Server=Yes
🏓 Received enhanced peer discovery ping - sending handshake response
🤝 Sending Enhanced Handshake Response

👥 Enhanced Peer Network Status:
Current Mode: WiFi Primary | WiFi: YourHotspot | Server: Reachable
  1. ESP2_SENSOR_002 (user_bob) [ESP2_UNIVERSAL v2.0.0]
     RSSI: -50 dBm | Handshake: ✓ | Validated: ✓ | Last seen: 3s ago
     Capabilities: peer_discovery, enhanced_handshake, mode_switching, wifi_scanning
     Preferred Mode: WiFi Primary
```

## Configuration Options

### Timing Settings
```cpp
const unsigned long PEER_DISCOVERY_INTERVAL = 10000;  // Ping every 10s
const unsigned long DATA_SEND_INTERVAL = 30000;       // Data every 30s
const unsigned long WIFI_SCAN_INTERVAL = 60000;       // WiFi scan every 60s
const unsigned long SERVER_CHECK_INTERVAL = 120000;   // Server check every 2min
```

### Security Settings
```cpp
const int HANDSHAKE_TIMEOUT = 30000;              // 30s handshake timeout
const int MAX_HANDSHAKE_ATTEMPTS = 3;             // Max retry attempts
const char* DEVICE_TYPE = "ESP2_UNIVERSAL";       // Device type validation
```

### Network Settings
```cpp
const char* TEST_SERVER_HOST = "8.8.8.8";         // Connectivity test server
const char* WEBSOCKET_SERVER_IP = "192.168.137.1"; // Monitor server
const int MAX_PEERS = 15;                         // Maximum tracked peers
```

## Message Envelope Structure (Enhanced)

```json
{
  "version": "2.0",
  "message_id": "ping_1700000000_1",
  "timestamp": 1700000000,
  "shared_key": "ESP2_NETWORK_KEY",
  "source_device": {
    "device_id": "ESP2_SENSOR_001",
    "owner": "user_alice",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "device_type": "ESP2_UNIVERSAL",
    "firmware_version": "2.0.0"
  },
  "message_type": "ping",
  "payload": {
    "rssi": -45,
    "free_heap": 234567,
    "uptime": 12345,
    "communication_mode": 2,
    "wifi_connected": true,
    "server_reachable": true,
    "connected_ssid": "YourHotspot",
    "wifi_channel": 6,
    "capabilities": [
      "peer_discovery", 
      "enhanced_messaging", 
      "mode_switching", 
      "wifi_scanning"
    ]
  }
}
```

## Enhanced Security Features
- **Device Type Validation**: Only accepts messages from ESP2_UNIVERSAL devices
- **Firmware Version Tracking**: Monitors peer firmware compatibility
- **Trust Management**: Tracks peer validation and trust status
- **Handshake Timeout**: Prevents hanging handshake attempts
- **Advanced Loop Prevention**: Sophisticated attempt tracking

## Next Phases

### Phase 4: Triangulation & Relative Positioning
- RSSI-based distance estimation using enhanced peer data
- Relative positioning calculations with multiple reference points
- Direction finding (N/S/E/W) using signal strength triangulation

### Phase 5: Message Relaying & Holding
- Store and forward messaging through peer network
- Multi-hop relay chains with path optimization
- Message persistence in flash storage
- Delivery confirmation and retry logic

### Phase 6: Robustness & Optimization
- Advanced error handling and recovery
- Memory optimization and resource management
- Enhanced security with encryption
- Performance monitoring and analytics

## Troubleshooting

### WiFi Connection Issues
1. Verify WiFi credentials in knownNetworks array
2. Check that networks are in range and operational
3. Monitor serial output for connection attempts

### Peer Discovery Problems
1. Ensure all devices use the same SHARED_KEY and DEVICE_TYPE
2. Check that devices are within ESP-NOW range (~100m)
3. Verify protocol version compatibility

### Mode Switching Issues
1. Check server reachability configuration
2. Monitor WiFi scan results and network availability
3. Verify mode switch cooldown timing

### Enhanced Handshake Failures
1. Check device type and firmware version compatibility
2. Monitor handshake timeout and retry logic
3. Verify peer validation and trust establishment

## Files Structure
```
ESP2_Universal_Phase1/
├── ESP2_Universal_Phase1.ino    # Enhanced firmware (Phase 1-3)
└── README.md                    # This documentation

docs/
└── envelope_schema.md           # Enhanced message format documentation
```

## Performance Notes
- **Memory Usage**: Enhanced peer tracking uses more RAM (~2-3KB per peer)
- **WiFi Scanning**: Periodic scans may briefly interrupt ESP-NOW communication
- **Mode Switching**: Brief communication gaps during mode transitions
- **Handshake Overhead**: Enhanced validation increases initial connection time