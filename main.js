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

      // Detect ESP-NOW relay messages from USB
      let source = parsedData.source || 'USB';
      const mode = parsedData.mode || '';
      
      // Simple relay detection - just look for the key patterns
      const isESPNowRelay = message.includes('sender_mac') && message.includes('received_data');
      
      if (isESPNowRelay) {
        console.log('ESP-NOW relay detected via brute force method!');
      }
      
      // Update relay connection status when ESP-NOW message detected
      if (isESPNowRelay) {
        console.log('ESP-NOW relay detected via USB! Setting indicator...');
        console.log('Message contains sender_mac:', !!parsedData.sender_mac);
        console.log('Message contains received_data:', !!parsedData.received_data);
        
        source = 'RELAY_USB';
        connections.relayUsb.connected = true;
        
        console.log('connections.relayUsb.connected set to:', connections.relayUsb.connected);
        
        // Clear any existing timeout
        if (connections.relayUsb.timeout) {
          clearTimeout(connections.relayUsb.timeout);
        }
        
        // Set timeout to clear indicator after 10 seconds of inactivity
        connections.relayUsb.timeout = setTimeout(() => {
          console.log('Timeout: Clearing USB relay indicator after 10 seconds');
          connections.relayUsb.connected = false;
          sendConnectionStatus();
        }, 10000);
        
        // Send status with a small delay to ensure it's processed
        setTimeout(() => {
          sendConnectionStatus();
        }, 100);
        
        console.log('RelayUSB status set to:', connections.relayUsb.connected);
      }
      
      sendToRenderer('log', {
        message: message,
        source: source,
        mode: mode,
        isESPNowRelay: isESPNowRelay,
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

function sendConnectionStatus() {
  // Create a clean copy for sending (without timeout references)
  const cleanConnections = {
    wifi: connections.wifi,
    relayUsb: { connected: connections.relayUsb.connected },
    relayWifi: { connected: connections.relayWifi.connected },
    usb: connections.usb
  };
  sendToRenderer('connection-status', cleanConnections);
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
