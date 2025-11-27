# ESP2 Message Envelope Schema (Phase 1-3)

## Overview
All ESP2 communication uses a standardized JSON envelope structure that supports nested envelopes for different message types. This schema has been enhanced through Phase 1-3 development to include WiFi/server awareness, advanced peer validation, and capability negotiation.

## Base Envelope Structure (Enhanced)

```json
{
  "version": "2.0",
  "message_id": "unique_message_identifier",
  "timestamp": 1700000000,
  "shared_key": "ESP2_NETWORK_KEY",
  "source_device": {
    "device_id": "ESP2_SENSOR_001",
    "owner": "user_name",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "device_type": "ESP2_UNIVERSAL",
    "firmware_version": "2.0.0"
  },
  "message_type": "ping|handshake|data|relay",
  "payload": {
    // Message-specific content with Phase 2/3 enhancements
  }
}
```

## Required Fields (Enhanced)
- `version`: String - Protocol version for future compatibility (now "2.0")
- `message_id`: String - Unique identifier for this message
- `timestamp`: Number - Unix timestamp when message was created
- `shared_key`: String - Authentication key for peer validation
- `source_device`: Object - Enhanced information about the sending device
  - `device_id`: String - Unique device identifier
  - `owner`: String - Device owner name
  - `mac_address`: String - Device MAC address
  - `device_type`: String - Device type for validation (Phase 3)
  - `firmware_version`: String - Firmware version (Phase 3)
- `message_type`: String - Type of message being sent
- `payload`: Object - Message-specific data with enhanced capabilities

## Enhanced Message Types

### 1. Enhanced Ping Message (Phase 2/3)
Used for ESP2 peer discovery with WiFi and capability awareness.

```json
{
  "version": "2.0",
  "message_id": "ping_12345",
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
    "peer_count": 3,
    "connected_ssid": "YourHotspot",
    "wifi_channel": 6,
    "wifi_rssi": -40,
    "capabilities": [
      "peer_discovery", 
      "enhanced_messaging", 
      "mode_switching", 
      "wifi_scanning",
      "wifi_communication",
      "server_access"
    ]
  }
}
```

#### New Ping Fields (Phase 2/3):
- `communication_mode`: Number - Current communication mode (0-3)
- `wifi_connected`: Boolean - WiFi connection status
- `server_reachable`: Boolean - Server connectivity status
- `peer_count`: Number - Number of known peers
- `connected_ssid`: String - Connected WiFi network name
- `wifi_channel`: Number - WiFi channel number
- `wifi_rssi`: Number - WiFi signal strength
- `capabilities`: Array - Enhanced capability list

### 2. Enhanced Handshake Response (Phase 3)
Advanced handshake with capability negotiation and validation.

```json
{
  "version": "2.0",
  "message_id": "handshake_67890",
  "timestamp": 1700000001,
  "shared_key": "ESP2_NETWORK_KEY",
  "source_device": {
    "device_id": "ESP2_SENSOR_002",
    "owner": "user_bob",
    "mac_address": "BB:CC:DD:EE:FF:AA",
    "device_type": "ESP2_UNIVERSAL",
    "firmware_version": "2.0.0"
  },
  "message_type": "handshake",
  "payload": {
    "reply_to": "ping_12345",
    "rssi": -50,
    "free_heap": 198765,
    "uptime": 6789,
    "communication_mode": 1,
    "wifi_connected": false,
    "server_reachable": false,
    "capabilities": [
      "peer_discovery",
      "enhanced_handshake",
      "mode_switching",
      "wifi_scanning"
    ],
    "validation": {
      "trusted": true,
      "validation_timestamp": 1700000001
    }
  }
}
```

#### New Handshake Fields (Phase 3):
- `communication_mode`: Number - Peer's current mode
- `wifi_connected`: Boolean - Peer's WiFi status
- `server_reachable`: Boolean - Peer's server status
- `validation`: Object - Trust and validation information
  - `trusted`: Boolean - Whether this peer is trusted
  - `validation_timestamp`: Number - When validation occurred

### 3. Enhanced Data Message (Phase 2/3)
Regular sensor/status data with network awareness.

```json
{
  "version": "2.0",
  "message_id": "data_11111",
  "timestamp": 1700000002,
  "shared_key": "ESP2_NETWORK_KEY",
  "source_device": {
    "device_id": "ESP2_SENSOR_001",
    "owner": "user_alice",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "device_type": "ESP2_UNIVERSAL",
    "firmware_version": "2.0.0"
  },
  "message_type": "data",
  "payload": {
    "sensor_data": {
      "temperature": 23.5,
      "humidity": 65.2
    },
    "system_data": {
      "free_heap": 234000,
      "uptime": 12400,
      "peer_count": 3,
      "communication_mode": 2,
      "wifi_connected": true,
      "server_reachable": true,
      "connected_ssid": "YourHotspot",
      "wifi_rssi": -42
    },
    "network_status": {
      "trusted_peers": 2,
      "validated_peers": 2
    }
  }
}
```

#### New Data Fields (Phase 2/3):
- `system_data`: Enhanced with communication status
  - `communication_mode`: Number - Current mode
  - `wifi_connected`: Boolean - WiFi status
  - `server_reachable`: Boolean - Server status
  - `connected_ssid`: String - WiFi network name
  - `wifi_rssi`: Number - WiFi signal strength
- `network_status`: Object - Peer network summary
  - `trusted_peers`: Number - Count of trusted peers
  - `validated_peers`: Number - Count of validated peers

### 4. Relay Message (Ready for Phase 5)
Structure prepared for future message relaying functionality.

```json
{
  "version": "2.0",
  "message_id": "relay_22222",
  "timestamp": 1700000003,
  "shared_key": "ESP2_NETWORK_KEY",
  "source_device": {
    "device_id": "ESP2_SENSOR_002",
    "owner": "user_bob",
    "mac_address": "BB:CC:DD:EE:FF:AA",
    "device_type": "ESP2_UNIVERSAL",
    "firmware_version": "2.0.0"
  },
  "message_type": "relay",
  "payload": {
    "relay_info": {
      "original_sender": "ESP2_SENSOR_003",
      "relay_path": ["ESP2_SENSOR_003", "ESP2_SENSOR_002"],
      "relay_timestamp": 1700000003,
      "relay_reason": "no_wifi_connection",
      "hop_count": 1
    },
    "original_message": {
      // Complete original envelope nested here
    }
  }
}
```

## Communication Modes (Phase 2)

Communication mode values used in messages:

- `0`: MODE_ESP_NOW_ONLY - ESP-NOW only (no WiFi available)
- `1`: MODE_WIFI_BACKUP - WiFi available, ESP-NOW primary
- `2`: MODE_WIFI_PRIMARY - WiFi + server available, WiFi primary  
- `3`: MODE_WIFI_ONLY - WiFi exclusive mode

## Enhanced Capabilities (Phase 2/3)

Standard capability identifiers:

### Core Capabilities
- `"peer_discovery"` - Basic peer discovery protocol
- `"enhanced_messaging"` - Phase 2/3 envelope support
- `"relay"` - Message relaying (Phase 5)

### Phase 2 Capabilities
- `"mode_switching"` - Dynamic communication mode switching
- `"wifi_scanning"` - WiFi network scanning
- `"wifi_communication"` - WiFi-based communication
- `"server_access"` - Server connectivity

### Phase 3 Capabilities  
- `"enhanced_handshake"` - Advanced handshake protocol
- `"peer_validation"` - Peer trust validation
- `"capability_negotiation"` - Dynamic capability exchange

### Future Capabilities (Phase 4+)
- `"triangulation"` - Position estimation
- `"positioning"` - Relative positioning
- `"message_holding"` - Store and forward
- `"encryption"` - Message encryption

## Security Enhancements (Phase 3)

### Device Type Validation
The `device_type` field must match `"ESP2_UNIVERSAL"` for message acceptance.

### Firmware Version Tracking
The `firmware_version` field enables compatibility checking and feature negotiation.

### Trust Management
Peers maintain trust status through the validation system:
- Initial contact: untrusted
- Successful handshake: trusted
- Failed handshakes: untrusted
- Validation timeout: requires re-validation

## Message ID Generation (Enhanced)

Message IDs follow the pattern: `<type>_<timestamp>_<counter>`

Examples:
- `ping_1700000000_1`
- `handshake_1700000001_2` 
- `data_1700000002_3`
- `relay_1700000003_4`

## Version Evolution

### Version 1.0 (Phase 1)
- Basic envelope structure
- Simple peer discovery
- Basic handshake protocol

### Version 2.0 (Phase 2/3)
- Enhanced device information
- WiFi/server awareness
- Advanced peer validation
- Capability negotiation
- Communication mode tracking

### Future Versions
- Version 3.0: Triangulation and positioning
- Version 4.0: Message relaying and persistence
- Version 5.0: Advanced security and encryption

## Backward Compatibility

The system warns about version mismatches but allows communication between different versions when possible. Critical changes will increment the major version number.

## Message Size Considerations

- Basic ping: ~300-500 bytes
- Enhanced handshake: ~400-600 bytes  
- Data message: ~500-800 bytes
- Relay message: Variable (original + overhead)

ESP-NOW has a 250-byte limit, so large messages may need fragmentation in future phases.