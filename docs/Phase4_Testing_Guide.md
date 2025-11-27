# Phase 4 Triangulation Testing Guide

## Quick Test Setup

### Hardware Required
- 3-4 ESP32 devices flashed with ESP2_Universal_Phase1.ino
- Serial monitors for each device
- Known reference positions (e.g., corners of a room)

### Basic Testing Procedure

#### 1. Deploy Devices
```
Device A: Corner 1 (Reference: 0,0)
Device B: Corner 2 (Reference: 3,0) - 3 meters East
Device C: Corner 3 (Reference: 0,3) - 3 meters North  
Device D: Center   (Reference: 1.5,1.5) - Should detect as center
```

#### 2. Monitor Serial Output
Look for these key indicators:

**Phase 4 Startup Messages:**
```
✓ RSSI-based Distance Estimation
✓ Relative Positioning (N/S/E/W)  
✓ Triangulation Algorithm
```

**Positioning Status:**
```
📐 Triangulation Status: Ready (3+ peers available)
📍 Relative Positions:
  Device_B: 2.8m East (confidence: 0.85)
  Device_C: 3.1m North (confidence: 0.79)
```

#### 3. Expected Behaviors

**Distance Estimation:**
- Should see ~3m distances between corner devices
- Center device should show ~2.1m to corners
- RSSI values should be consistent (±5 dBm)

**Direction Detection:**
- Device A should see B as "East" and C as "North"
- Device D should see appropriate compass directions
- Confidence should be >0.7 for good positioning

**Triangulation Messages:**
```
📏 Received triangulation data
📊 Updated position for Device_B: 2.8m East
```

#### 4. Validation Steps

1. **Check Peer Discovery:**
   - All devices should discover each other
   - Look for "triangulation" in capabilities list

2. **Monitor RSSI Stability:**
   - RSSI values should be relatively stable (±10 dBm)
   - Distance estimates should converge over time

3. **Verify Direction Detection:**
   - Manually verify compass directions match physical layout
   - Check that confidence values are reasonable

4. **Test Position Updates:**
   - Move a device and observe position changes
   - Verify triangulation reconvergence

### Troubleshooting

**No Triangulation:**
- Check that 3+ devices are in range
- Verify ESP-NOW communication working
- Look for "triangulation" capability in peer discovery

**Poor Accuracy:**
- Adjust `RSSI_REFERENCE_POWER` calibration
- Check for interference (WiFi, Bluetooth)
- Ensure stable power supply to all devices

**Missing Position Data:**
- Check protocol version compatibility (should be 4.0)
- Verify triangulation interval timing
- Monitor peer validation success

### Expected Serial Output Example
```
🚀 Phase 1: Core Communication ✓
🔄 Phase 2: WiFi/Server Scanning ✓  
🤝 Phase 3: Enhanced Peer Discovery ✓
📐 Phase 4: Triangulation & Positioning ✓

📡 Broadcasting Enhanced Peer Discovery Ping (Phase 4)
Mode: ESP-NOW Only | WiFi: Disconnected | Server: Unreachable | Triangulation: Ready

📍 Position Summary (3 peers with valid positions):
  Device_B: 2.8m East (confidence: 0.85)
  Device_C: 3.1m North (confidence: 0.79)  
  Device_D: 2.1m Southeast (confidence: 0.72)
My estimated position: (1.2, 1.4) confidence: 0.78
```

This indicates successful Phase 4 triangulation operation!