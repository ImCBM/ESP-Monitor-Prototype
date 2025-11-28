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

// Connection states - Updated for ESP1 Gateway Architecture
let connections = {
  wifi: { connected: false, signalStrength: 0, distance: 0 },  // Direct WiFi ESP devices (if any)
  esp1Gateway: { connected: false, channel: 1 },              // ESP1 wired gateway status
  esp2ViaGateway: { connected: false, deviceCount: 0 },       // ESP2 devices via ESP1
  usb: { connected: false }                                   // USB serial connection to ESP1
};

// ESP Gateway Monitoring - ESP1 Gateway Architecture
let gatewayStats = {
  esp1Connected: false,
  esp1Uptime: 0,
  esp2DeviceCount: 0,
  esp2ActiveDevices: new Set(),  // Track unique ESP2 device IDs
  messageStats: {
    ping: 0, handshake: 0, data: 0, triangulation: 0,
    relay: 0, wifiScan: 0, optimization: 0, unknown: 0,
    total: 0, delivered: 0
  },
  gatewayInfo: {
    deviceId: 'Unknown', version: 'Unknown', uptime: 0,
    protocolRange: 'Unknown', lastSender: 'None', lastMessageType: 'None',
    channel: 1, macAddress: 'Unknown'
  }
};

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

// Format JSON messages for better readability
function formatMessage(message) {
  try {
    // Try to parse as JSON
    const parsed = JSON.parse(message);
    // If successful, return formatted JSON
    return JSON.stringify(parsed, null, 2);
  } catch (e) {
    // Check if it looks like JSON but might have formatting issues
    if (message.includes('{') && message.includes('}') && message.includes('"')) {
      try {
        // Attempt to clean and parse common JSON formatting issues
        let cleaned = message.trim();
        // Remove any trailing characters after the last }
        const lastBrace = cleaned.lastIndexOf('}');
        if (lastBrace !== -1) {
          cleaned = cleaned.substring(0, lastBrace + 1);
        }
        const parsed = JSON.parse(cleaned);
        return JSON.stringify(parsed, null, 2);
      } catch (e2) {
        // If still fails, return original message
        return message;
      }
    }
    // Not JSON, return as-is
    return message;
  }
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

  mainWindow.loadFile('index.html');
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
        const formattedMessage = formatMessage(message);
        let parsedData;
        
        try {
          parsedData = JSON.parse(message);
        } catch (e) {
          parsedData = { raw: message };
        }

        // Determine source (WiFi devices only - no ESP2 via WiFi in new architecture)
        let source = parsedData.source || 'WIFI';
        const mode = parsedData.mode || '';
        
        // Update connection status for direct WiFi devices
        if (source === 'WIFI') {
          connections.wifi.connected = true;
          connections.wifi.signalStrength = parsedData.rssi || 0;
          connections.wifi.distance = estimateDistance(parsedData.rssi || 0);
        }

        // Note: ESP-NOW messages from ESP2 devices should NOT come via WebSocket
        // They should only come via ESP1 gateway through USB Serial
        if (parsedData.message_type || parsedData.version) {
          console.warn('ESP2-like message received via WebSocket - should come through ESP1 gateway instead');
        }

        sendConnectionStatus();
        
        sendToRenderer('log', {
          message: formattedMessage,
          source: source,
          mode: mode,
          isESPNowRelay: false,  // No ESP-NOW via WebSocket in new architecture
          timestamp: new Date().toISOString(),
          distance: connections.wifi.distance || 0,
          data: parsedData
        });
      } catch (error) {
        console.error('Error processing WebSocket message:', error);
      }
    });

    ws.on('close', () => {
      console.log('WebSocket client disconnected');
      connections.wifi.connected = false;
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
      const formattedMessage = formatMessage(message);
      let parsedData;
      
      try {
        parsedData = JSON.parse(message);
      } catch (e) {
        parsedData = { raw: message };
      }

      // Debug logging for relay detection (only when ESP-NOW detected)
      if (message.includes('sender_mac') || parsedData.sender_mac) {
        console.log('Processing potential relay message:', {
          message: message.substring(0, 100) + '...',
          hasSenderMac: !!parsedData.sender_mac,
          hasReceivedData: !!parsedData.received_data,
          hasRelayedData: !!parsedData.relayed_data,
          hasESPNowInMessage: parsedData.message && parsedData.message.includes('ESP-NOW'),
          hasJSONRelay: message.includes('sender_mac') && message.includes('received_data'),
          source: parsedData.source
        });
      }

      // Detect ESP1 Gateway vs ESP2 messages coming through ESP1
      let source = parsedData.source || 'USB';
      const mode = parsedData.mode || '';
      
      // ESP1 Gateway Detection - Enhanced for async operation
      const isESP1Gateway = message.includes('ESP1_WIRED_GATEWAY') || 
                           message.includes('ESP1_WIRED_GATEWAY_ASYNC') ||
                           message.includes('gateway_type') ||
                           message.includes('🔔 ESP-NOW DATA RECEIVED!') ||
                           message.includes('📡 ESP2 MESSAGE RECEIVED') ||
                           message.includes('📡 ESP2 MESSAGE ASYNC PROCESSING') ||
                           message.includes('POWERED_ASYNC') ||
                           parsedData.device_id === 'ESP1_WIRED_GATEWAY' ||
                           parsedData.deviceType === 'ESP1_WIRED_GATEWAY_ASYNC';
      
      // ESP2 Message Detection - Enhanced for envelope structure
      const isESP2Message = (parsedData.message_type && parsedData.version && parsedData.source_device) ||
                           (message.includes('📤 USB:') && parsedData.message_type) ||
                           (parsedData.source_device?.device_id && parsedData.source_device.device_id.includes('ESP2'));
      
      // Process ESP1 Gateway Status Messages
      if (isESP1Gateway) {
        source = 'ESP1_GATEWAY';
        connections.esp1Gateway.connected = true;
        gatewayStats.esp1Connected = true;
        
        // Handle ESP1 debug messages with async support
        if (message.includes('🔔 ESP-NOW DATA RECEIVED!')) {
          source = 'ESP1_DEBUG';
          console.log('ESP1 received ESP-NOW data - processing...');
        } else if (message.includes('📡 ESP2 MESSAGE ASYNC PROCESSING')) {
          source = 'ESP1_DEBUG_ASYNC';
          console.log('ESP1 async processing ESP2 message');
        } else if (message.includes('📡 ESP2 MESSAGE RECEIVED')) {
          source = 'ESP1_GATEWAY';
          console.log('ESP1 gateway processed ESP2 message');
        } else if (message.includes('POWERED_ASYNC') || message.includes('Dual-core')) {
          source = 'ESP1_GATEWAY_ASYNC';
          console.log('ESP1 async gateway status received');
        } else {
          processESP1GatewayMessage(parsedData);
          console.log('ESP1 Gateway status received:', parsedData.device_id || parsedData.deviceType || 'Status Update');
        }
      }
      
      // Process ESP2 Messages (received via ESP1 gateway)
      if (isESP2Message) {
        source = 'ESP2_VIA_GATEWAY';
        connections.esp2ViaGateway.connected = true;
        
        // Extract ESP2 device info from forwarded message
        let esp2DeviceId = 'unknown';
        if (parsedData.source_device?.device_id) {
          esp2DeviceId = parsedData.source_device.device_id;
        }
        
        processESP2Message(parsedData);
        
        // Track unique ESP2 devices
        if (esp2DeviceId !== 'unknown') {
          gatewayStats.esp2ActiveDevices.add(esp2DeviceId);
          connections.esp2ViaGateway.deviceCount = gatewayStats.esp2ActiveDevices.size;
        }
        
        console.log(`ESP2 ${parsedData.message_type || 'message'} from ${esp2DeviceId} via ESP1 gateway (Phase ${getESP2Phase(parsedData)})}`);
        
        // Clear timeout for ESP2 activity
        if (connections.esp2ViaGateway.timeout) {
          clearTimeout(connections.esp2ViaGateway.timeout);
        }
        // Set timeout to clear indicator after 30 seconds of inactivity
        connections.esp2ViaGateway.timeout = setTimeout(() => {
          connections.esp2ViaGateway.connected = false;
          sendConnectionStatus();
        }, 30000);
      }
      
      sendToRenderer('log', {
        message: formattedMessage,
        source: source,
        mode: mode,
        isESPNowRelay: isESP2Message,  // True only for ESP2 messages coming through ESP1
        timestamp: new Date().toISOString(),
        data: parsedData
      });
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
  if (parsedData.deviceType) {
    gatewayStats.gatewayInfo.deviceType = parsedData.deviceType;
  }
  if (parsedData.esp1_version) {
    gatewayStats.gatewayInfo.version = parsedData.esp1_version;
  }
  if (parsedData.esp2_protocol_range) {
    gatewayStats.gatewayInfo.protocolRange = parsedData.esp2_protocol_range;
  }
  if (parsedData.uptime) {
    gatewayStats.gatewayInfo.uptime = parsedData.uptime;
    gatewayStats.esp1Uptime = parsedData.uptime;
  }
  if (parsedData.gatewayMode) {
    gatewayStats.gatewayInfo.gatewayMode = parsedData.gatewayMode;
  }
  
  // Update async processing info if available
  if (parsedData.asyncQueueDepth !== undefined) {
    gatewayStats.gatewayInfo.asyncQueueDepth = parsedData.asyncQueueDepth;
  }
  if (parsedData.asyncQueueCapacity !== undefined) {
    gatewayStats.gatewayInfo.asyncQueueCapacity = parsedData.asyncQueueCapacity;
  }
  if (parsedData.coreUtilization) {
    gatewayStats.gatewayInfo.coreUtilization = parsedData.coreUtilization;
  }
  if (parsedData.taskInfo) {
    gatewayStats.gatewayInfo.taskInfo = parsedData.taskInfo;
  }
  
  // Update message statistics from gateway
  if (parsedData.message_stats) {
    Object.assign(gatewayStats.messageStats, parsedData.message_stats);
  }
  if (parsedData.messagesReceived) {
    gatewayStats.messageStats.total = parsedData.messagesReceived;
  }
  if (parsedData.messagesDelivered) {
    gatewayStats.messageStats.delivered = parsedData.messagesDelivered;
  }
  
  // Update gateway health info
  if (parsedData.gateway_health) {
    gatewayStats.gatewayInfo.lastSender = parsedData.gateway_health.last_sender || 'None';
    gatewayStats.gatewayInfo.lastMessageType = parsedData.gateway_health.last_message_type || 'None';
    gatewayStats.gatewayInfo.macAddress = parsedData.gateway_health.esp_now_mac || 'Unknown';
  }
  
  // Update ESP1 gateway connection info
  connections.esp1Gateway.connected = true;
  connections.esp1Gateway.channel = 1;  // Fixed channel for ESP-NOW
}

function processESP2Message(parsedData) {
  // Update ESP2 message statistics
  const messageType = parsedData.message_type || 'unknown';
  if (gatewayStats.messageStats.hasOwnProperty(messageType)) {
    gatewayStats.messageStats[messageType]++;
  } else {
    gatewayStats.messageStats.unknown++;
  }
  
  gatewayStats.messageStats.total++;
  gatewayStats.messageStats.delivered++;
  
  // Track ESP2 devices coming through gateway
  if (parsedData.source_device?.device_id) {
    const deviceId = parsedData.source_device.device_id;
    gatewayStats.esp2ActiveDevices.add(deviceId);
    gatewayStats.esp2DeviceCount = gatewayStats.esp2ActiveDevices.size;
    
    // Update connection info
    connections.esp2ViaGateway.deviceCount = gatewayStats.esp2DeviceCount;
    
    console.log(`ESP2 device ${deviceId} active via gateway (total: ${gatewayStats.esp2DeviceCount})`);
  }
}

function getESP2Phase(parsedData) {
  // Determine ESP2 phase from message type
  const messageType = parsedData.message_type;
  if (!messageType) return 'Unknown';
  
  const phaseMap = {
    'ping': 1, 'data': 1,
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
    esp1Gateway: connections.esp1Gateway,
    esp2ViaGateway: { 
      connected: connections.esp2ViaGateway.connected,
      deviceCount: connections.esp2ViaGateway.deviceCount || 0
    },
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
