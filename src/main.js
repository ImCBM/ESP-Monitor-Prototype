const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const WebSocket = require('ws');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

let mainWindow;
let wss;
let serialPort = null;
let serialParser = null;
const WEBSOCKET_PORT = 8080;

// Connection states
let connections = {
  wifi: { connected: false, signalStrength: 0, distance: 0 },
  relayUsb: { connected: false },
  relayWifi: { connected: false },
  usb: { connected: false }
};

// ESP Gateway Monitoring
let gatewayStats = {
  esp1Connected: false,
  esp2DeviceCount: 0,
  messageStats: {
    ping: 0, handshake: 0, data: 0, triangulation: 0,
    relay: 0, distance: 0, wifiScan: 0, optimization: 0, unknown: 0,
    total: 0, delivered: 0
  },
  gatewayInfo: {
    deviceId: 'Unknown', version: 'Unknown', uptime: 0,
    protocolRange: 'Unknown', lastSender: 'None', lastMessageType: 'None'
  }
};

// ============== ESP2 DEVICE TRACKING & PROCESSING SYSTEM ==============
// This is where the Monitor does all the heavy lifting that ESPs don't do

// Registry of all known ESP2 devices with their data
let esp2DeviceRegistry = {};

// Triangulation data storage
let triangulationData = {
  referencePoints: [], // Devices with known/fixed positions
  measurements: [],    // Recent distance measurements for triangulation
  lastCalculation: null
};

// Distance measurement history for averaging/smoothing
let distanceHistory = {};  // { deviceId: { targetId: [measurements] } }

// Configuration for distance calculation
const distanceConfig = {
  txPower: -59,           // Calibrated RSSI at 1 meter
  pathLossExponent: 2.5,  // Indoor environment (2.0=free space, 3.0-4.0=heavy obstructions)
  historySize: 10,        // Number of measurements to keep for averaging
  confidenceThreshold: 0.5 // Minimum confidence to use measurement
};

// Calculate distance from RSSI using Log-Distance Path Loss Model
function calculateDistanceFromRSSI(rssi) {
  if (rssi === 0 || rssi === undefined || rssi >= 0) return null;
  const distance = Math.pow(10, (distanceConfig.txPower - rssi) / (10 * distanceConfig.pathLossExponent));
  return Math.round(distance * 100) / 100; // Round to 2 decimal places
}

// Calculate confidence based on signal strength
function calculateConfidence(rssi) {
  if (rssi >= -50) return 0.95;  // Excellent signal
  if (rssi >= -60) return 0.85;  // Good signal
  if (rssi >= -70) return 0.70;  // Fair signal
  if (rssi >= -80) return 0.50;  // Weak signal
  return 0.30;                    // Very weak signal
}

// Add distance measurement to history and return smoothed average
function addDistanceMeasurement(fromDevice, toDevice, rssi) {
  const key = `${fromDevice}->${toDevice}`;
  
  if (!distanceHistory[key]) {
    distanceHistory[key] = [];
  }
  
  const distance = calculateDistanceFromRSSI(rssi);
  const confidence = calculateConfidence(rssi);
  
  if (distance !== null) {
    distanceHistory[key].push({
      distance,
      rssi,
      confidence,
      timestamp: Date.now()
    });
    
    // Keep only recent measurements
    if (distanceHistory[key].length > distanceConfig.historySize) {
      distanceHistory[key].shift();
    }
  }
  
  // Calculate weighted average using confidence
  const measurements = distanceHistory[key];
  if (measurements.length === 0) return null;
  
  let totalWeight = 0;
  let weightedSum = 0;
  
  measurements.forEach(m => {
    weightedSum += m.distance * m.confidence;
    totalWeight += m.confidence;
  });
  
  return {
    distance: Math.round((weightedSum / totalWeight) * 100) / 100,
    rawDistance: distance,
    confidence: confidence,
    sampleCount: measurements.length,
    rssi: rssi
  };
}

// Update or create device in registry
function updateDeviceRegistry(deviceData, gatewayRssi) {
  const deviceId = deviceData.d || deviceData.device_id;
  if (!deviceId) return null;
  
  const now = Date.now();
  
  if (!esp2DeviceRegistry[deviceId]) {
    esp2DeviceRegistry[deviceId] = {
      deviceId: deviceId,
      owner: deviceData.o || deviceData.owner || 'Unknown',
      mac: deviceData.m || deviceData.mac_address || 'Unknown',
      firstSeen: now,
      lastSeen: now,
      messageCount: 0,
      distanceToGateway: null,
      freeHeap: null,
      uptime: null,
      peerCount: 0,
      peers: {},          // { peerId: { rssi, distance, lastSeen } }
      lastMessageType: null,
      protocolVersion: deviceData.v || 'Unknown'
    };
  }
  
  const device = esp2DeviceRegistry[deviceId];
  device.lastSeen = now;
  device.messageCount++;
  device.protocolVersion = deviceData.v || device.protocolVersion;
  device.owner = deviceData.o || device.owner;
  device.mac = deviceData.m || device.mac;
  
  // Update distance to gateway from RSSI
  if (gatewayRssi && gatewayRssi < 0) {
    const distData = addDistanceMeasurement(deviceId, 'ESP1_GATEWAY', gatewayRssi);
    if (distData) {
      device.distanceToGateway = distData;
    }
  }
  
  // Update device stats based on message type
  const msgType = deviceData.y;
  device.lastMessageType = typeCodeToString(msgType);
  
  // Extract common fields (compact format)
  if (deviceData.h !== undefined) device.freeHeap = deviceData.h;
  if (deviceData.u !== undefined) device.uptime = deviceData.u;
  if (deviceData.n !== undefined) device.peerCount = deviceData.n;
  
  return device;
}

// Process triangulation data from ESP2 message
function processTriangulationData(deviceData, gatewayRssi) {
  const sourceDevice = deviceData.d;
  const peerArray = deviceData.pa || [];
  
  if (peerArray.length === 0) return null;
  
  const triangulationEntry = {
    sourceDevice: sourceDevice,
    timestamp: Date.now(),
    gatewayRssi: gatewayRssi,
    gatewayDistance: calculateDistanceFromRSSI(gatewayRssi),
    peers: []
  };
  
  // Process each peer in the triangulation data
  peerArray.forEach(peer => {
    const peerId = peer.d;
    const peerRssi = peer.r;
    
    if (peerId && peerRssi) {
      const distData = addDistanceMeasurement(sourceDevice, peerId, peerRssi);
      
      triangulationEntry.peers.push({
        deviceId: peerId,
        rssi: peerRssi,
        distance: distData ? distData.distance : calculateDistanceFromRSSI(peerRssi),
        confidence: distData ? distData.confidence : calculateConfidence(peerRssi)
      });
      
      // Also update the device registry's peer info
      if (esp2DeviceRegistry[sourceDevice]) {
        esp2DeviceRegistry[sourceDevice].peers[peerId] = {
          rssi: peerRssi,
          distance: distData ? distData.distance : calculateDistanceFromRSSI(peerRssi),
          lastSeen: Date.now()
        };
      }
    }
  });
  
  // Store for potential position calculation
  triangulationData.measurements.push(triangulationEntry);
  
  // Keep only recent measurements
  if (triangulationData.measurements.length > 50) {
    triangulationData.measurements.shift();
  }
  
  return triangulationEntry;
}

// Process distance measurement message
function processDistanceMessage(deviceData, gatewayRssi) {
  const sourceDevice = deviceData.d;
  const targetDevice = deviceData.to;
  const rssi = deviceData.r;
  
  if (!targetDevice || rssi === undefined) return null;
  
  const distData = addDistanceMeasurement(sourceDevice, targetDevice, rssi);
  
  // Update peer info in device registry
  if (esp2DeviceRegistry[sourceDevice] && distData) {
    esp2DeviceRegistry[sourceDevice].peers[targetDevice] = {
      rssi: rssi,
      distance: distData.distance,
      confidence: distData.confidence,
      lastSeen: Date.now()
    };
  }
  
  return {
    from: sourceDevice,
    to: targetDevice,
    rssi: rssi,
    distance: distData ? distData.distance : calculateDistanceFromRSSI(rssi),
    confidence: distData ? distData.confidence : calculateConfidence(rssi),
    smoothed: distData ? distData.sampleCount > 1 : false
  };
}

// Process peer array (pa) from ping/data messages - updates device registry with peer distances
function processPeerArray(sourceDevice, peerArray) {
  if (!peerArray || peerArray.length === 0) return [];
  
  const processedPeers = [];
  
  peerArray.forEach(peer => {
    const peerId = peer.d;
    const peerRssi = peer.r;
    
    if (peerId && peerRssi !== undefined) {
      const distData = addDistanceMeasurement(sourceDevice, peerId, peerRssi);
      
      const peerInfo = {
        deviceId: peerId,
        rssi: peerRssi,
        distance: distData ? distData.distance : calculateDistanceFromRSSI(peerRssi),
        confidence: distData ? distData.confidence : calculateConfidence(peerRssi)
      };
      
      processedPeers.push(peerInfo);
      
      // Update device registry's peer info
      if (esp2DeviceRegistry[sourceDevice]) {
        esp2DeviceRegistry[sourceDevice].peers[peerId] = {
          rssi: peerRssi,
          distance: peerInfo.distance,
          confidence: peerInfo.confidence,
          lastSeen: Date.now()
        };
      }
    }
  });
  
  return processedPeers;
}

// Get all device distances for UI display
function getAllDeviceDistances() {
  const distances = {};
  
  Object.values(esp2DeviceRegistry).forEach(device => {
    distances[device.deviceId] = {
      toGateway: device.distanceToGateway,
      toPeers: device.peers,
      lastSeen: device.lastSeen,
      online: (Date.now() - device.lastSeen) < 30000 // Consider online if seen in last 30s
    };
  });
  
  return distances;
}

// Get triangulation-ready status
function getTriangulationStatus() {
  const onlineDevices = Object.values(esp2DeviceRegistry).filter(
    d => (Date.now() - d.lastSeen) < 30000
  );
  
  return {
    deviceCount: onlineDevices.length,
    ready: onlineDevices.length >= 3,
    devices: onlineDevices.map(d => ({
      id: d.deviceId,
      distanceToGateway: d.distanceToGateway?.distance || null,
      peerCount: Object.keys(d.peers).length
    })),
    recentMeasurements: triangulationData.measurements.slice(-10)
  };
}
// ============== END ESP2 DEVICE TRACKING SYSTEM ==============

// Estimate distance from RSSI (in meters)
// Formula: distance = 10^((TxPower - RSSI) / (10 * N))
// TxPower: typical ESP32 transmission power at 1m = -59 dBm
// N: path loss exponent (2 = free space, 2-4 = indoor)
function estimateDistance(rssi) {
  if (rssi === 0 || rssi >= 0) return 0;
  
  const txPower = -59; // Measured RSSI at 1 meter
  const n = 2.5; // Path loss exponent (indoor environment)
  
  const distance = Math.pow(10, (txPower - rssi) / (10 * n));
  return Math.round(distance * 10) / 10; // Round to 1 decimal
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    },
    backgroundColor: '#1e1e1e',
    title: 'ESP32 Monitor'
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));
}

// WebSocket Server for WiFi ESPs
function startWebSocketServer() {
  wss = new WebSocket.Server({ port: WEBSOCKET_PORT });

  wss.on('listening', () => {
    console.log(`WebSocket server listening on port ${WEBSOCKET_PORT}`);
    sendToRenderer('log', {
      message: `WebSocket server started on port ${WEBSOCKET_PORT}`,
      source: 'SYSTEM',
      timestamp: new Date().toISOString()
    });
  });

  wss.on('connection', (ws, req) => {
    const clientIP = req.socket.remoteAddress;
    console.log(`WebSocket client connected from ${clientIP}`);

    ws.on('message', (data) => {
      try {
        const message = data.toString();
        let parsedData;
        
        try {
          parsedData = JSON.parse(message);
        } catch (e) {
          parsedData = { raw: message };
        }

        // Determine source (WiFi or Relay)
        let source = parsedData.source || 'WIFI';
        const mode = parsedData.mode || '';
        
        // Update connection status
        if (source === 'WIFI') {
          connections.wifi.connected = true;
          connections.wifi.signalStrength = parsedData.rssi || 0;
          connections.wifi.distance = estimateDistance(parsedData.rssi || 0);
        }

        // Detect ESP-NOW relay messages (WiFi path)
        const isESPNowRelay = parsedData.sender_mac || 
                             parsedData.relayed_data ||
                             parsedData.received_data ||
                             (parsedData.message && parsedData.message.includes('ESP-NOW message'));
        if (isESPNowRelay) {
          source = 'RELAY_WIFI';
          connections.relayWifi.connected = true;
          // Clear any existing timeout
          if (connections.relayWifi.timeout) {
            clearTimeout(connections.relayWifi.timeout);
          }
          // Set timeout to clear indicator after 10 seconds of inactivity
          connections.relayWifi.timeout = setTimeout(() => {
            connections.relayWifi.connected = false;
            sendConnectionStatus();
          }, 10000);
        }

        sendConnectionStatus();
        
        sendToRenderer('log', {
          message: message,
          source: source,
          mode: mode,
          isESPNowRelay: isESPNowRelay,
          timestamp: new Date().toISOString(),
          distance: connections.wifi.distance || connections.relay.distance || 0,
          data: parsedData
        });
      } catch (error) {
        console.error('Error processing WebSocket message:', error);
      }
    });

    ws.on('close', () => {
      console.log('WebSocket client disconnected');
      connections.wifi.connected = false;
      connections.relay.connected = false;
      sendConnectionStatus();
      sendToRenderer('log', {
        message: 'WebSocket client disconnected',
        source: 'SYSTEM',
        timestamp: new Date().toISOString()
      });
      
      // Auto-reconnect is handled by ESP trying to reconnect
    });

    ws.on('error', (error) => {
      console.error('WebSocket error:', error);
      sendToRenderer('log', {
        message: `WebSocket error: ${error.message}`,
        source: 'ERROR',
        timestamp: new Date().toISOString()
      });
    });
  });

  wss.on('error', (error) => {
    console.error('WebSocket server error:', error);
  });
}

// Serial Port Communication
async function listSerialPorts() {
  try {
    const ports = await SerialPort.list();
    return ports;
  } catch (error) {
    console.error('Error listing serial ports:', error);
    return [];
  }
}

function openSerialPort(portPath) {
  if (serialPort && serialPort.isOpen) {
    serialPort.close();
  }

  serialPort = new SerialPort({
    path: portPath,
    baudRate: 115200,
    autoOpen: false
  });

  serialParser = serialPort.pipe(new ReadlineParser({ delimiter: '\n' }));

  serialPort.open((error) => {
    if (error) {
      console.error('Error opening serial port:', error);
      connections.usb.connected = false;
      sendConnectionStatus();
      sendToRenderer('log', {
        message: `Failed to open serial port: ${error.message}`,
        source: 'ERROR',
        timestamp: new Date().toISOString()
      });
      
      // Auto-reconnect after 3 seconds
      setTimeout(() => {
        if (!serialPort.isOpen) {
          openSerialPort(portPath);
        }
      }, 3000);
      return;
    }

    connections.usb.connected = true;
    sendConnectionStatus();
    sendToRenderer('log', {
      message: `Serial port opened: ${portPath}`,
      source: 'SYSTEM',
      timestamp: new Date().toISOString()
    });
  });

  serialParser.on('data', (data) => {
    try {
      const message = data.trim();
      if (!message) return;
      
      let parsedData;
      let isValidJSON = false;
      
      try {
        parsedData = JSON.parse(message);
        isValidJSON = true;
      } catch (e) {
        // Handle non-JSON messages (like status lines)
        parsedData = { raw: message };
      }

      // Skip processing decorative lines and status messages
      if (message.includes('═══') || message.includes('🔄') || message.includes('📡') || 
          message.includes('✅') || message.startsWith('Phase:') || message.startsWith('From ESP2:') ||
          message.startsWith('Message Type:') || message.startsWith('Sender Device:') ||
          message.startsWith('Total Messages:') || message.startsWith('Gateway Status:')) {
        return; // Skip these fragmented display messages
      }

      // Detect ESP1 Gateway vs ESP2 messages
      let source = parsedData.source || 'USB';
      const mode = parsedData.mode || '';
      
      // ESP1 Gateway Detection (prioritize consolidated messages)
      const isESP1Gateway = isValidJSON && (
        parsedData.gateway_type === 'ESP1_WIRED_GATEWAY' ||
        parsedData.device_id === 'ESP1_WIRED_GATEWAY' ||
        source === 'ESP1_GATEWAY'
      );
      
      // ESP2 Message Detection (direct from ESP2 or relayed) - but NOT if it's embedded in ESP1 Gateway
      const isESP2Message = isValidJSON && (
        parsedData.message_type ||
        parsedData.version ||
        parsedData.source_device
      ) && !isESP1Gateway;
      
      const isESPNowRelay = isESP2Message;
      
      // Process ESP1 Gateway Status Messages
      if (isESP1Gateway) {
        source = 'ESP1_GATEWAY';
        gatewayStats.esp1Connected = true;
        processESP1GatewayMessage(parsedData);
        
        // If this gateway message contains ESP2 data, process it but don't create separate log entry
        if (parsedData.esp2_raw_data) {
          try {
            const esp2Data = JSON.parse(parsedData.esp2_raw_data);
            // Pass gateway RSSI for distance calculation
            const gatewayRssi = parsedData.esp2_rssi || null;
            const processedResult = processESP2Message(esp2Data, gatewayRssi);
            
            // Attach processed data to parsedData for renderer
            parsedData._esp2Processed = processedResult;
            parsedData._deviceRegistry = esp2DeviceRegistry[esp2Data.d];
            parsedData._allDistances = getAllDeviceDistances();
            parsedData._triangulationStatus = getTriangulationStatus();
          } catch (e) {
            console.log('Could not parse embedded ESP2 data:', e.message);
          }
        }
        
        console.log(`ESP1 Gateway: ${parsedData.esp2_message_type || 'Status'} from ${parsedData.esp2_sender_device || 'Unknown ESP2'}`);
      }
      
      // Process ESP2 Messages (Phase 1-6) - ONLY if not already processed via ESP1 Gateway
      if (isESPNowRelay && !isESP1Gateway) {
        source = 'RELAY_USB';
        connections.relayUsb.connected = true;
        processESP2Message(parsedData);
        console.log(`ESP2 ${parsedData.message_type || 'message'} from ${parsedData.source_device?.device_id || 'unknown'} (Phase ${getESP2Phase(parsedData)})`);
        
        // Clear any existing timeout
        if (connections.relayUsb.timeout) {
          clearTimeout(connections.relayUsb.timeout);
        }
        // Set timeout to clear indicator after 10 seconds of inactivity
        connections.relayUsb.timeout = setTimeout(() => {
          connections.relayUsb.connected = false;
          sendConnectionStatus();
        }, 10000);
      }
      
      // Send to renderer - ESP1 Gateway messages take priority, standalone ESP2 only if no gateway
      if (isValidJSON && isESP1Gateway) {
        // Always send ESP1 Gateway messages (they may contain ESP2 data)
        sendToRenderer('log', {
          message: message,
          source: source,
          mode: mode,
          isESPNowRelay: isESPNowRelay,
          timestamp: new Date().toISOString(),
          data: parsedData
        });
      } else if (isValidJSON && isESP2Message && !isESP1Gateway) {
        // Send standalone ESP2 messages only if they're not from gateway
        sendToRenderer('log', {
          message: message,
          source: source,
          mode: mode,
          isESPNowRelay: isESPNowRelay,
          timestamp: new Date().toISOString(),
          data: parsedData
        });
      }
    } catch (error) {
      console.error('Error processing serial data:', error);
    }
  });

  serialPort.on('close', () => {
    console.log('Serial port closed');
    connections.usb.connected = false;
    sendConnectionStatus();
    sendToRenderer('log', {
      message: 'Serial port disconnected',
      source: 'SYSTEM',
      timestamp: new Date().toISOString()
    });

    // Auto-reconnect after 3 seconds
    setTimeout(() => {
      if (!serialPort.isOpen) {
        openSerialPort(portPath);
      }
    }, 3000);
  });

  serialPort.on('error', (error) => {
    console.error('Serial port error:', error);
    sendToRenderer('log', {
      message: `Serial port error: ${error.message}`,
      source: 'ERROR',
      timestamp: new Date().toISOString()
    });
  });
}

function sendToRenderer(channel, data) {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send(channel, data);
  }
}

// ESP Message Processing Functions
function processESP1GatewayMessage(parsedData) {
  // Update gateway information
  if (parsedData.device_id) {
    gatewayStats.gatewayInfo.deviceId = parsedData.device_id;
  }
  if (parsedData.esp1_version) {
    gatewayStats.gatewayInfo.version = parsedData.esp1_version;
  }
  if (parsedData.esp2_protocol_range) {
    gatewayStats.gatewayInfo.protocolRange = parsedData.esp2_protocol_range;
  }
  if (parsedData.uptime) {
    gatewayStats.gatewayInfo.uptime = parsedData.uptime;
  }
  
  // Update gateway health info from consolidated message
  if (parsedData.esp2_sender_device) {
    gatewayStats.gatewayInfo.lastSender = parsedData.esp2_sender_device;
  }
  if (parsedData.esp2_message_type) {
    gatewayStats.gatewayInfo.lastMessageType = parsedData.esp2_message_type;
  }
  if (parsedData.message_count) {
    gatewayStats.messageStats.total = parsedData.message_count;
  }
  
  // Update message statistics from gateway
  if (parsedData.message_stats) {
    Object.assign(gatewayStats.messageStats, parsedData.message_stats);
  }
  
  // Update gateway health info
  if (parsedData.gateway_health) {
    gatewayStats.gatewayInfo.lastSender = parsedData.gateway_health.last_sender || 'None';
    gatewayStats.gatewayInfo.lastMessageType = parsedData.gateway_health.last_message_type || 'None';
  }
}

// Convert compact type code to verbose type string
function typeCodeToString(code) {
  const typeMap = {
    0: 'ping',
    1: 'data', 
    2: 'handshake',
    3: 'triangulation',
    4: 'relay',
    5: 'distance'
  };
  return typeMap[code] || 'unknown';
}

// RSSI to distance calculation (moved from ESPs to Monitor)
function rssiToDistance(rssi, txPower = -59, pathLossExponent = 2.0) {
  if (rssi === 0 || rssi === undefined) return -1;
  // Using Log-Distance Path Loss Model: d = 10 ^ ((TxPower - RSSI) / (10 * n))
  const distance = Math.pow(10, (txPower - rssi) / (10 * pathLossExponent));
  return Math.round(distance * 100) / 100; // Round to 2 decimal places
}

function processESP2Message(parsedData, gatewayRssi = null) {
  // Handle both compact (y) and verbose (message_type) formats
  let messageType;
  let typeCode = parsedData.y;
  
  if (typeCode !== undefined) {
    // Compact format: y = type code (0-5)
    messageType = typeCodeToString(typeCode);
  } else {
    // Verbose format: message_type = string
    messageType = parsedData.message_type || 'unknown';
  }
  
  // Update ESP2 message statistics
  if (gatewayStats.messageStats.hasOwnProperty(messageType)) {
    gatewayStats.messageStats[messageType]++;
  } else {
    gatewayStats.messageStats.unknown++;
  }
  
  gatewayStats.messageStats.total++;
  gatewayStats.messageStats.delivered++;
  
  // Update ESP2 device count - handle both compact (d) and verbose (source_device) formats
  let deviceId = null;
  if (parsedData.d) {
    // Compact format: d = device_id string
    deviceId = parsedData.d;
  } else if (parsedData.source_device?.device_id) {
    // Verbose format
    deviceId = parsedData.source_device.device_id;
  }
  
  if (deviceId) {
    gatewayStats.esp2DeviceCount = Math.max(gatewayStats.esp2DeviceCount, 
      parseInt(deviceId.replace(/\D/g, '')) || 1);
  }
  
  // ===== COMPREHENSIVE ESP2 MESSAGE PROCESSING =====
  // Update device registry with all device info
  const device = updateDeviceRegistry(parsedData, gatewayRssi);
  
  // Process based on message type
  let processedData = {
    messageType: messageType,
    deviceId: deviceId,
    timestamp: Date.now()
  };
  
  switch(typeCode) {
    case 0: // ping
      // Basic health check - device info already updated
      processedData.heap = parsedData.h;
      processedData.uptime = parsedData.u;
      processedData.peers = parsedData.n;
      if (gatewayRssi) {
        processedData.distanceToGateway = calculateDistanceFromRSSI(gatewayRssi);
      }
      // Process peer array if present (pa contains RSSI to other ESP2 devices)
      if (parsedData.pa && parsedData.pa.length > 0) {
        processedData.peerDistances = processPeerArray(deviceId, parsedData.pa);
      }
      break;
      
    case 1: // data
      // System data message
      processedData.heap = parsedData.h;
      processedData.uptime = parsedData.u;
      processedData.peers = parsedData.n;
      if (gatewayRssi) {
        processedData.distanceToGateway = calculateDistanceFromRSSI(gatewayRssi);
      }
      // Process peer array if present
      if (parsedData.pa && parsedData.pa.length > 0) {
        processedData.peerDistances = processPeerArray(deviceId, parsedData.pa);
      }
      break;
      
    case 2: // handshake
      processedData.handshakeOk = parsedData.ok;
      processedData.replyTo = parsedData.re;
      break;
      
    case 3: // triangulation
      // Process triangulation data with all peer distances
      const triData = processTriangulationData(parsedData, gatewayRssi);
      if (triData) {
        processedData.triangulation = triData;
        processedData.peerDistances = triData.peers;
      }
      break;
      
    case 4: // relay
      processedData.relayId = parsedData.ri;
      processedData.originSender = parsedData.os;
      processedData.hopCount = parsedData.hc;
      processedData.messageData = parsedData.md;
      processedData.relayCount = parsedData.rc;
      break;
      
    case 5: // distance
      // Process direct distance measurement between two devices
      const distResult = processDistanceMessage(parsedData, gatewayRssi);
      if (distResult) {
        processedData.distanceMeasurement = distResult;
      }
      break;
  }
  
  // Attach processed data to the original for UI
  parsedData._processed = processedData;
  parsedData._deviceRegistry = esp2DeviceRegistry[deviceId];
  parsedData._allDistances = getAllDeviceDistances();
  parsedData._triangulationStatus = getTriangulationStatus();
  
  return processedData;
}

function getESP2Phase(parsedData) {
  // Determine ESP2 phase from message type
  // Handle both compact (y) and verbose (message_type) formats
  let messageType;
  if (parsedData.y !== undefined) {
    // Compact format: y = type code (0-5)
    messageType = typeCodeToString(parsedData.y);
  } else {
    messageType = parsedData.message_type;
  }
  
  if (!messageType) return 'Unknown';
  
  const phaseMap = {
    'ping': 1, 'data': 1, 'distance': 1,
    'wifi_scan': 2,
    'handshake': 3,
    'triangulation': 4,
    'relay': 5,
    'optimization': 6
  };
  
  return phaseMap[messageType] || parsedData.data?.phase || 'Unknown';
}

function sendConnectionStatus() {
  // Create a clean copy for sending (without timeout references)
  const cleanConnections = {
    wifi: connections.wifi,
    relayUsb: { connected: connections.relayUsb.connected },
    relayWifi: { connected: connections.relayWifi.connected },
    usb: connections.usb
  };
  sendToRenderer('connection-status', cleanConnections);
  
  // Also send gateway statistics
  sendToRenderer('gateway-stats', gatewayStats);
}

// IPC Handlers
ipcMain.handle('list-serial-ports', async () => {
  return await listSerialPorts();
});

ipcMain.handle('open-serial-port', async (event, portPath) => {
  openSerialPort(portPath);
  return { success: true };
});

ipcMain.handle('close-serial-port', async () => {
  if (serialPort && serialPort.isOpen) {
    serialPort.close();
  }
  return { success: true };
});

ipcMain.handle('get-connection-status', async () => {
  return connections;
});

ipcMain.handle('clear-log', async () => {
  return { success: true };
});

// Gateway Monitoring IPC Handlers
ipcMain.handle('get-gateway-stats', async () => {
  return gatewayStats;
});

ipcMain.handle('reset-gateway-stats', async () => {
  gatewayStats.messageStats = {
    ping: 0, handshake: 0, data: 0, triangulation: 0,
    relay: 0, wifiScan: 0, optimization: 0, unknown: 0,
    total: 0, delivered: 0
  };
  return { success: true };
});

// ESP2 Device Registry & Distance Tracking IPC Handlers
ipcMain.handle('get-esp2-devices', async () => {
  return esp2DeviceRegistry;
});

ipcMain.handle('get-all-distances', async () => {
  return getAllDeviceDistances();
});

ipcMain.handle('get-triangulation-status', async () => {
  return getTriangulationStatus();
});

ipcMain.handle('get-distance-history', async () => {
  return distanceHistory;
});

ipcMain.handle('clear-device-registry', async () => {
  esp2DeviceRegistry = {};
  distanceHistory = {};
  triangulationData.measurements = [];
  return { success: true };
});

// App lifecycle
app.whenReady().then(() => {
  createWindow();
  startWebSocketServer();
  
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (wss) {
    wss.close();
  }
  if (serialPort && serialPort.isOpen) {
    serialPort.close();
  }
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
