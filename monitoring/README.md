# ESP Gateway Monitoring Dashboard

## Overview

This enhanced monitoring system provides comprehensive real-time monitoring for the ESP1 Wired Gateway and ESP2 battery-powered network ecosystem. It's specifically designed to track the Phase 1-6 ESP2 protocol and the simplified ESP1 wired gateway architecture.

## Features

### 🔌 ESP1 Wired Gateway Monitoring
- **Real-time Gateway Status**: Connection health, uptime, version info
- **USB Serial Connection**: Direct monitoring of wired ESP1 gateway
- **ESP-NOW Reception Tracking**: Monitor ESP-NOW message reception
- **Protocol Compatibility**: Support for ESP2 protocol versions 1.0-6.0
- **Message Persistence**: Track message recovery and persistence status

### 📊 ESP2 Phase Statistics
- **Phase 1**: Core communication (ping, data messages)
- **Phase 2**: WiFi coordination messages
- **Phase 3**: Enhanced peer discovery (handshakes)
- **Phase 4**: RSSI triangulation data
- **Phase 5**: Store-and-forward message relaying
- **Phase 6**: Network optimization messages

### 🌐 Network Topology Visualization
- **Real-time Device Discovery**: Visual representation of ESP2 devices
- **Connection Mapping**: Show communication paths through ESP1 gateway
- **Phase-based Color Coding**: Different colors for each protocol phase
- **Interactive Device Info**: Hover for detailed device statistics

### 📨 Message Activity Monitoring
- **Real-time Message Log**: Live feed of all ESP2 communications
- **Message Type Classification**: Automatic categorization by phase and type
- **Delivery Tracking**: Monitor successful USB Serial delivery
- **Pause/Resume Controls**: Stop message flow for analysis

## Architecture

### ESP1 Wired Gateway
```
ESP2 Network → ESP1 (ESP-NOW) → USB Serial → Monitor Dashboard
```

**Key Characteristics:**
- No WiFi dependencies - pure wired operation
- Direct USB Serial connection to monitoring PC
- ESP-NOW reception from battery ESP2 devices
- Message persistence for reliability
- Universal ESP2 protocol support (Phases 1-6)

### ESP2 Battery Network
**Supported Phases:**
1. **Phase 1**: Core envelope messaging and peer discovery
2. **Phase 2**: WiFi scanning coordination messages  
3. **Phase 3**: Enhanced peer validation and handshake protocols
4. **Phase 4**: RSSI triangulation and positioning data
5. **Phase 5**: Store-and-forward message relaying
6. **Phase 6**: Network optimization and robustness

## File Structure

```
monitoring/
├── gateway-monitor.html     # Main monitoring dashboard
├── gateway-monitor.css      # Responsive UI styles
├── gateway-monitor.js       # Real-time monitoring logic
└── README.md               # This documentation
```

## Usage

### 1. Setup ESP1 Wired Gateway
- Upload `ESP1_Smart_Dual_Mode.ino` to ESP32
- Connect ESP1 via USB to monitoring PC
- Ensure ESP2 devices are powered and in range

### 2. Launch Dashboard
```bash
# Open the monitoring dashboard
open monitoring/gateway-monitor.html
```

### 3. Monitor ESP2 Network
- View real-time ESP2 message activity
- Track phase-specific statistics
- Monitor network topology changes
- Analyze message delivery patterns

## Integration with Electron App

For full integration with the main Electron monitoring application:

```javascript
// main.js integration example
const { ipcMain } = require('electron');

// Handle gateway monitoring requests
ipcMain.handle('get-gateway-status', async () => {
    return {
        gatewayOnline: serialPort?.isOpen || false,
        esp2DeviceCount: activeESP2Devices.size,
        messageStats: currentMessageStats
    };
});

// Forward ESP2 messages to gateway monitor
serialParser.on('data', (data) => {
    const messageData = parseESP2Message(data);
    if (messageData) {
        mainWindow.webContents.send('esp2-message', messageData);
    }
});
```

## Key Monitoring Metrics

### Gateway Health
- **USB Connection**: Direct serial connection status
- **ESP-NOW Reception**: Message reception capability
- **Protocol Compatibility**: Version mismatch tracking
- **Uptime**: Gateway operational duration

### ESP2 Network Activity
- **Message Volume**: Total messages by phase
- **Device Discovery**: Active ESP2 device count
- **Delivery Success**: USB Serial forwarding rate
- **Network Topology**: Device relationships and communication paths

### Message Analysis
- **Phase Distribution**: Message breakdown by ESP2 phase
- **Device Activity**: Per-device communication patterns
- **Relay Effectiveness**: Store-and-forward success rates
- **Protocol Evolution**: Version compatibility tracking

## Troubleshooting

### Gateway Connection Issues
1. **USB Serial Problems**: Check COM port availability and drivers
2. **ESP-NOW Reception**: Verify ESP1 MAC address and channel
3. **Protocol Mismatches**: Update ESP2 devices to compatible versions

### Network Visibility Issues  
1. **Missing ESP2 Devices**: Check power and proximity to ESP1
2. **Message Loss**: Verify ESP-NOW channel alignment
3. **Phase Detection**: Ensure ESP2 firmware includes phase headers

### Performance Optimization
1. **Message Volume**: Use pause controls for detailed analysis
2. **Browser Performance**: Clear message log periodically
3. **Network Scale**: Monitor device count vs. performance

## Future Enhancements

- **Historical Analytics**: Message pattern analysis over time
- **Performance Metrics**: Latency and throughput measurements
- **Alert System**: Automated notifications for network issues
- **Export Capabilities**: CSV/JSON data export for analysis
- **Advanced Filtering**: Phase and device-specific message filtering

## Protocol Compatibility

**Supported ESP2 Versions**: 1.0 - 6.0  
**ESP1 Gateway Version**: 5.0.0  
**Monitoring Dashboard**: Real-time Phase 1-6 support

This monitoring system provides complete visibility into the simplified ESP1 wired gateway and ESP2 battery network ecosystem, enabling effective troubleshooting and optimization of the entire communication infrastructure.