// DOM elements
const logPanel = document.getElementById('log-panel');
const serialPortSelect = document.getElementById('serial-port-select');
const connectBtn = document.getElementById('connect-btn');
const disconnectBtn = document.getElementById('disconnect-btn');
const clearBtn = document.getElementById('clear-btn');
const refreshOverviewBtn = document.getElementById('refresh-overview');
const autoScrollCheck = document.getElementById('auto-scroll');
const filterSelect = document.getElementById('filter-select');
const resetStatsBtn = document.getElementById('reset-stats');

// Status indicators
const wifiStatus = document.getElementById('wifi-status');
const relayUsbStatus = document.getElementById('relay-usb-status');
const relayWifiStatus = document.getElementById('relay-wifi-status');
const usbStatus = document.getElementById('usb-status');

// Dashboard elements
const navItems = document.querySelectorAll('.nav-item');
const tabContents = document.querySelectorAll('.tab-content');
const systemHealthIndicator = document.getElementById('system-health');
const gatewayDeviceElement = document.getElementById('gateway-device');
const esp2CountElement = document.getElementById('esp2-count');
const totalMessagesElement = document.getElementById('total-messages');
const systemUptimeElement = document.getElementById('system-uptime');
const recentActivityFeed = document.getElementById('recent-activity');
const networkConnections = document.getElementById('network-connections');

// Phase count elements
const phaseCountElements = {
  1: document.getElementById('phase1-count'),
  3: document.getElementById('phase3-count'),
  4: document.getElementById('phase4-count'),
  5: document.getElementById('phase5-count')
};

// Device detail elements
const gatewayDeviceDetail = document.getElementById('gateway-device-detail');
const gatewayVersion = document.getElementById('gateway-version');
const gatewayProtocol = document.getElementById('gateway-protocol');
const esp1Status = document.getElementById('esp1-status');

// Network analysis elements
const wifiSignalDetail = document.getElementById('wifi-signal-detail');
const wifiDistanceDetail = document.getElementById('wifi-distance-detail');
const messageRate = document.getElementById('message-rate');
const signalBars = document.querySelectorAll('.signal-bar');

// Application state
let isConnected = false;
let connectionStatus = {};
let logFilter = 'ALL';
let autoScroll = true;
let startTime = Date.now();
let messageCount = 0;
let lastMessageTime = 0;
let gatewayStats = {
  esp1Connected: false,
  esp2DeviceCount: 0,
  messageStats: { total: 0 },
  gatewayInfo: { deviceId: 'Unknown' }
};

// Initialize dashboard
document.addEventListener('DOMContentLoaded', () => {
  initializeDashboard();
  loadSerialPorts();
  updateSystemUptime();
  setInterval(updateSystemUptime, 1000);
  setInterval(updateMessageRate, 1000);
});

function initializeDashboard() {
  // Set up tab navigation
  navItems.forEach(item => {
    item.addEventListener('click', () => {
      const tabId = item.dataset.tab;
      switchTab(tabId);
    });
  });
  
  // Set default active tab
  switchTab('overview');
  
  // Setup event listeners
  setupEventListeners();
}

function switchTab(tabId) {
  // Remove active class from all nav items and tab contents
  navItems.forEach(item => item.classList.remove('active'));
  tabContents.forEach(content => content.classList.remove('active'));
  
  // Add active class to selected tab
  const selectedNavItem = document.querySelector(`[data-tab="${tabId}"]`);
  const selectedTabContent = document.getElementById(`${tabId}-tab`);
  
  if (selectedNavItem && selectedTabContent) {
    selectedNavItem.classList.add('active');
    selectedTabContent.classList.add('active');
  }
}

function updateSystemUptime() {
  const uptime = Math.floor((Date.now() - startTime) / 1000);
  const hours = Math.floor(uptime / 3600);
  const minutes = Math.floor((uptime % 3600) / 60);
  const seconds = uptime % 60;
  
  const uptimeString = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
  if (systemUptimeElement) {
    systemUptimeElement.textContent = uptimeString;
  }
}

function updateMessageRate() {
  const now = Date.now();
  const timeDiff = (now - lastMessageTime) / 1000;
  const rate = timeDiff > 0 ? Math.round(messageCount / timeDiff * 10) / 10 : 0;
  
  if (messageRate) {
    messageRate.textContent = `${rate}/sec`;
  }
}

function updateDashboard() {
  // Update system health indicator
  let healthColor = '🔴';
  if (gatewayStats.esp1Connected) {
    healthColor = gatewayStats.esp2DeviceCount > 0 ? '🟢' : '🟡';
  }
  if (systemHealthIndicator) {
    systemHealthIndicator.textContent = healthColor;
  }
  
  // Update overview metrics
  if (gatewayDeviceElement) {
    gatewayDeviceElement.textContent = gatewayStats.gatewayInfo.deviceId;
  }
  if (gatewayDeviceDetail) {
    gatewayDeviceDetail.textContent = gatewayStats.gatewayInfo.deviceId;
  }
  if (esp2CountElement) {
    esp2CountElement.textContent = gatewayStats.esp2DeviceCount;
  }
  if (totalMessagesElement) {
    totalMessagesElement.textContent = gatewayStats.messageStats.total || 0;
  }
  if (gatewayVersion) {
    gatewayVersion.textContent = gatewayStats.gatewayInfo.version || '-';
  }
  if (gatewayProtocol) {
    gatewayProtocol.textContent = gatewayStats.gatewayInfo.protocolRange || '-';
  }
  
  // Update ESP1 status
  if (esp1Status) {
    esp1Status.textContent = gatewayStats.esp1Connected ? '🟢' : '🔴';
  }
  
  // Update phase counts
  Object.keys(phaseCountElements).forEach(phase => {
    const element = phaseCountElements[phase];
    if (element) {
      element.textContent = gatewayStats.messageStats[getPhaseMessageType(phase)] || 0;
    }
  });
}

function getPhaseMessageType(phase) {
  const phaseMap = {
    '1': 'ping',
    '3': 'handshake',
    '4': 'triangulation',
    '5': 'relay'
  };
  return phaseMap[phase] || 'unknown';
}

function addActivityItem(message, source) {
  if (!recentActivityFeed) return;
  
  const activityItem = document.createElement('div');
  activityItem.className = 'activity-item';
  
  const time = new Date().toLocaleTimeString();
  const shortMessage = message.length > 50 ? message.substring(0, 50) + '...' : message;
  
  activityItem.innerHTML = `
    <span class="activity-time">${time}</span>
    <span class="activity-message">${source}: ${shortMessage}</span>
  `;
  
  // Add to top of feed
  recentActivityFeed.insertBefore(activityItem, recentActivityFeed.firstChild);
  
  // Keep only last 10 items
  while (recentActivityFeed.children.length > 10) {
    recentActivityFeed.removeChild(recentActivityFeed.lastChild);
  }
}

function updateSignalBars(rssi) {
  if (!signalBars) return;
  
  // Convert RSSI to signal strength (0-4 bars)
  let strength = 0;
  if (rssi >= -50) strength = 4;
  else if (rssi >= -60) strength = 3;
  else if (rssi >= -70) strength = 2;
  else if (rssi >= -80) strength = 1;
  
  signalBars.forEach((bar, index) => {
    if (index < strength) {
      bar.classList.add('active');
    } else {
      bar.classList.remove('active');
    }
  });
}

function setupEventListeners() {
  // Event listeners
  connectBtn.addEventListener('click', connectSerial);
  disconnectBtn.addEventListener('click', disconnectSerial);
  clearBtn.addEventListener('click', clearLog);
  if (refreshOverviewBtn) {
    refreshOverviewBtn.addEventListener('click', () => {
      loadSerialPorts();
      updateDashboard();
    });
  }
  if (resetStatsBtn) {
    resetStatsBtn.addEventListener('click', resetGatewayStats);
  }
  filterSelect.addEventListener('change', (e) => {
    logFilter = e.target.value;
    filterLogEntries();
  });
  autoScrollCheck.addEventListener('change', (e) => {
    autoScroll = e.target.checked;
  });
}

async function resetGatewayStats() {
  try {
    await window.electronAPI.resetGatewayStats();
    addLogEntry('Gateway statistics reset', 'SYSTEM');
  } catch (error) {
    console.error('Error resetting gateway stats:', error);
    addLogEntry('Error resetting gateway stats', 'ERROR');
  }
}

// Functions
async function loadSerialPorts() {
  try {
    const ports = await window.electronAPI.listSerialPorts();
    serialPortSelect.innerHTML = '<option value="">Select Serial Port...</option>';
    
    ports.forEach(port => {
      const option = document.createElement('option');
      option.value = port.path;
      option.textContent = `${port.path} (${port.manufacturer || 'Unknown'})`;
      serialPortSelect.appendChild(option);
    });
  } catch (error) {
    console.error('Error loading serial ports:', error);
    addLogEntry('Error loading serial ports', 'ERROR');
  }
}

async function connectSerial() {
  const selectedPort = serialPortSelect.value;
  if (!selectedPort) {
    alert('Please select a serial port');
    return;
  }
  
  try {
    await window.electronAPI.openSerialPort(selectedPort);
    addLogEntry(`Connecting to ${selectedPort}...`, 'SYSTEM');
  } catch (error) {
    console.error('Error connecting to serial port:', error);
    addLogEntry('Error connecting to serial port', 'ERROR');
  }
}

async function disconnectSerial() {
  try {
    await window.electronAPI.closeSerialPort();
    addLogEntry('Disconnected from serial port', 'SYSTEM');
  } catch (error) {
    console.error('Error disconnecting from serial port:', error);
    addLogEntry('Error disconnecting from serial port', 'ERROR');
  }
}

async function clearLog() {
  logPanel.innerHTML = '';
  try {
    await window.electronAPI.clearLog();
  } catch (error) {
    console.error('Error clearing log:', error);
  }
}

// Distance calculation from RSSI
function calculateDistance(rssi) {
  // Path loss model: distance = 10^((TxPower - RSSI) / (10 * N))
  // TxPower: -40 dBm at 1m (calibrated for ESP32/ESP8266)
  // N: path loss exponent (2.0 for free space, 2.5-4.0 for indoor)
  const txPower = -40;
  const pathLossExponent = 2.5; // Indoor environment
  
  const distance = Math.pow(10, (txPower - rssi) / (10 * pathLossExponent));
  return Math.max(0.1, Math.min(100, distance)); // Clamp between 0.1m and 100m
}

function calculateConfidence(rssi) {
  // Confidence based on signal strength
  // Strong signal (-30 to -50) = high confidence
  // Weak signal (-80 to -100) = low confidence
  if (rssi >= -50) return 0.95;
  if (rssi >= -60) return 0.85;
  if (rssi >= -70) return 0.70;
  if (rssi >= -80) return 0.50;
  return 0.30;
}

// Convert compact type code to human-readable name
function typeCodeToName(typeCode) {
  const types = {
    0: 'Ping',
    1: 'Data',
    2: 'Handshake',
    3: 'Triangulation',
    4: 'Relay',
    5: 'Distance'
  };
  return types[typeCode] || `Unknown(${typeCode})`;
}

// Format uptime seconds to human-readable string
function formatUptime(seconds) {
  if (seconds < 60) return `${seconds}s`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ${seconds % 60}s`;
  const hours = Math.floor(seconds / 3600);
  const mins = Math.floor((seconds % 3600) / 60);
  return `${hours}h ${mins}m`;
}

// Helper function to get icon for message type
function getMessageTypeIcon(messageType) {
  const icons = {
    'ping': '🏓',
    'triangulation': '📍',
    'distance_measurement': '📏',
    'handshake': '🤝',
    'data': '📊',
    'wifi_scan': '📡',
    'relay': '🔄',
    'optimization': '⚡',
    'status': '📋'
  };
  return icons[messageType.toLowerCase()] || '📧';
}

function formatMessageData(data, source) {
  // Format ESP1 Gateway messages
  if (source === 'ESP1_GATEWAY' || data.gateway_type === 'ESP1_WIRED_GATEWAY') {
    return formatESP1GatewayMessage(data);
  }
  
  // Format ESP2 messages
  if (data.message_type || data.version || data.source_device) {
    return formatESP2Message(data);
  }
  
  // Default formatting for other messages
  if (data.raw) {
    return data.raw;
  }
  
  // Fallback to formatted JSON
  return `<pre>${JSON.stringify(data, null, 2)}</pre>`;
}

function formatESP1GatewayMessage(data) {
  const esp2Phase = data.esp2_phase || 'Unknown';
  const esp2Type = data.esp2_message_type || 'status';
  const esp2Device = data.esp2_sender_device || 'Unknown';
  const messageCount = data.message_count || 0;
  const rssi = data.esp2_rssi || 0;
  
  if (data.esp2_raw_data) {
    // This is an ESP2 message via ESP1 Gateway - format vertically
    try {
      const esp2Data = JSON.parse(data.esp2_raw_data);
      
      // Check if this is compact format (has 'y' type code) or verbose format
      const isCompact = esp2Data.y !== undefined;
      
      // Get device registry info for total message count
      const deviceReg = data._deviceRegistry;
      const totalMessages = deviceReg?.messageCount || messageCount;
      
      // ===== HEADER =====
      let formatted = `🔗 ESP1 Gateway: ${esp2Type.toUpperCase()} from ${esp2Device}\n`;
      formatted += `📶 RSSI: ${rssi} dBm\n`;
      formatted += `📊 Total Messages: ${totalMessages}\n`;
      formatted += `📊 Msg #${messageCount}\n`;
      formatted += `\n`;
      
      if (isCompact) {
        // ===== ESP2 DEVICE INFO (consolidated) =====
        formatted += `═══ ESP2 Device Info ═══\n`;
        formatted += `🏷️ Device ID: ${esp2Data.d || 'Unknown'}\n`;
        formatted += `👤 Owner: ${esp2Data.o || 'Unknown'}\n`;
        formatted += `📱 MAC: ${esp2Data.m || 'Unknown'}\n`;
        formatted += `📜 Protocol: v${esp2Data.v || '?'}\n`;
        formatted += `🔑 Message ID: ${esp2Data.i || 'Unknown'}\n`;
        formatted += `⏱️ Timestamp: ${esp2Data.t || 0}s\n`;
        formatted += `🌐 Network: ${esp2Data.k || 'Unknown'}\n`;
        
        // Add system info inline (heap, uptime) - from any message type that has it
        if (esp2Data.h !== undefined) formatted += `💾 Free Heap: ${esp2Data.h} KB\n`;
        if (esp2Data.u !== undefined) formatted += `⏱️ Uptime: ${formatUptime(esp2Data.u)}\n`;
        
        // ===== DISTANCE / TRIANGULATION STATUS (consolidated) =====
        formatted += `\n═══ Dist./Triangulation Status ═══\n`;
        
        // Calculate readiness
        const triStatus = data._triangulationStatus;
        const onlineDevices = triStatus?.deviceCount || 0;
        const distanceReady = onlineDevices >= 2;
        const triangleReady = onlineDevices >= 3;
        
        formatted += `📍 Distance Ready: ${distanceReady ? '✅ Yes (2+ devices)' : '❌ Need 2+ devices'}\n`;
        formatted += `📍 Triangle Ready: ${triangleReady ? '✅ Yes (3+ devices)' : '❌ Need 3+ devices'}\n`;
        
        // Distance from Gateway (with smoothed average if available)
        if (rssi && rssi < 0) {
          const distance = calculateDistance(rssi);
          const confidence = calculateConfidence(rssi);
          const avgDist = deviceReg?.distanceToGateway;
          
          if (avgDist && avgDist.sampleCount > 1) {
            formatted += `📏 Distance From Gateway: ${avgDist.distance.toFixed(2)}m avg (${avgDist.sampleCount} samples, ${(confidence * 100).toFixed(0)}% conf)\n`;
          } else {
            formatted += `📏 Distance From Gateway: ${distance.toFixed(2)}m (${(confidence * 100).toFixed(0)}% conf)\n`;
          }
        }
        
        // Nearby Peers section
        const peerCount = esp2Data.n || 0;
        const knownPeers = deviceReg?.peers || {};
        const peerEntries = Object.entries(knownPeers);
        
        formatted += `📡 Nearby Peers: ${peerCount}\n`;
        
        if (peerEntries.length > 0) {
          peerEntries.forEach(([peerId, peerData]) => {
            const peerConf = peerData.rssi ? calculateConfidence(peerData.rssi) : 0;
            const age = Math.round((Date.now() - peerData.lastSeen) / 1000);
            formatted += `   ${peerId}: ${peerData.distance?.toFixed(2) || '?'}m, ${peerData.rssi || '?'}dBm (${(peerConf * 100).toFixed(0)}% conf)\n`;
            formatted += `   └─ Last seen: ${age}s ago\n`;
          });
        }
        
        // Show triangulation peer array data if this is a triangulation message
        const typeCode = esp2Data.y;
        if (typeCode === 3 && esp2Data.pa && esp2Data.pa.length > 0) {
          formatted += `\n📍 Triangulation Peer Data:\n`;
          esp2Data.pa.forEach((peer, idx) => {
            if (peer.d && peer.r !== undefined) {
              const peerDist = calculateDistance(peer.r);
              const peerConf = calculateConfidence(peer.r);
              formatted += `   ${peer.d}: ${peerDist.toFixed(2)}m, ${peer.r}dBm (${(peerConf * 100).toFixed(0)}% conf)\n`;
            }
          });
        }
        
        // Show distance measurement if this is a distance message
        if (typeCode === 5 && esp2Data.to && esp2Data.r !== undefined) {
          const dist = calculateDistance(esp2Data.r);
          const conf = calculateConfidence(esp2Data.r);
          formatted += `\n📏 Distance Measurement:\n`;
          formatted += `   🎯 Target: ${esp2Data.to}\n`;
          formatted += `   📏 Distance: ${dist.toFixed(2)}m, ${esp2Data.r}dBm (${(conf * 100).toFixed(0)}% conf)\n`;
        }
        
        // Show handshake info if applicable
        if (typeCode === 2) {
          formatted += `\n🤝 Handshake:\n`;
          formatted += `   ✅ Status: ${esp2Data.ok ? 'OK' : 'Failed'}\n`;
          if (esp2Data.re) formatted += `   ↩️ Reply To: ${esp2Data.re}\n`;
        }
        
        // Show relay info if applicable
        if (typeCode === 4) {
          formatted += `\n🔄 Relay Info:\n`;
          if (esp2Data.ri) formatted += `   🔗 Relay ID: ${esp2Data.ri}\n`;
          if (esp2Data.os) formatted += `   📤 Origin: ${esp2Data.os}\n`;
          if (esp2Data.hc !== undefined) formatted += `   🔢 Hop Count: ${esp2Data.hc}\n`;
          if (esp2Data.md) formatted += `   📦 Data: ${esp2Data.md}\n`;
        }
        
      } else {
        // ===== VERBOSE FORMAT (legacy) =====
        // Extract key information from ESP2 data
        if (esp2Data.source_device) {
          formatted += `MAC: ${esp2Data.source_device.mac_address || 'Unknown'}\n`;
          formatted += `Type: ${esp2Data.source_device.device_type || 'Unknown'}\n`;
        }
        
        // Show sensor data if available
        if (esp2Data.payload?.sensor_data) {
          const sensor = esp2Data.payload.sensor_data;
          if (sensor.temperature !== undefined) {
            formatted += `Temperature: ${sensor.temperature}°C\n`;
          }
          if (sensor.humidity !== undefined) {
            formatted += `Humidity: ${sensor.humidity}%\n`;
          }
          if (sensor.system_data?.free_heap) {
            formatted += `Free Heap: ${sensor.system_data.free_heap}\n`;
          }
        }
        
        // Show ping data if available
        if (esp2Data.payload && esp2Data.message_type === 'ping') {
          if (esp2Data.payload.free_heap) {
            formatted += `Free Heap: ${esp2Data.payload.free_heap}\n`;
          }
          if (esp2Data.payload.peer_count !== undefined) {
            formatted += `Peers: ${esp2Data.payload.peer_count}\n`;
          }
          if (esp2Data.payload.positioning_ready !== undefined) {
            formatted += `Distance Meas.: ${esp2Data.payload.positioning_ready ? '✅ Ready' : '❌ Not Ready'}\n`;
          }
          if (esp2Data.payload.triangulation_ready !== undefined) {
            formatted += `Triangulation: ${esp2Data.payload.triangulation_ready ? '✅ Ready (3+ devs)' : '❌ Need 3+ devices'}\n`;
          }
          
          // Show peer status details with DISTANCE calculated from RSSI
          if (esp2Data.payload.peers_status && esp2Data.payload.peers_status.length > 0) {
            formatted += `\nPeer Details:\n`;
            esp2Data.payload.peers_status.forEach(peer => {
              const handshake = peer.handshake_complete ? '✅' : '❌';
              const validated = peer.validated ? '✅' : '❌';
              formatted += `  ${peer.device_id}: handshake:${handshake} Validation:${validated} (${peer.rssi}dBm)`;
              
              // Calculate distance from RSSI (monitor does the calculation)
              if (peer.rssi && peer.rssi < 0) {
                const distance = calculateDistance(peer.rssi);
                const confidence = calculateConfidence(peer.rssi);
                formatted += ` | 📏 ${distance.toFixed(1)}m (conf: ${(confidence * 100).toFixed(0)}%)`;
              }
              formatted += `\n`;
            });
          }
        }
        
        // Show data message nearby peers with distance
        if (esp2Data.payload && esp2Data.message_type === 'data') {
          if (esp2Data.payload.peer_count !== undefined) {
            formatted += `Peers: ${esp2Data.payload.peer_count}\n`;
          }
          if (esp2Data.payload.system_data?.uptime) {
            formatted += `Uptime: ${esp2Data.payload.system_data.uptime}s\n`;
          }
          
          // Show nearby peers with DISTANCE calculated from RSSI
          if (esp2Data.payload.nearby_peers && esp2Data.payload.nearby_peers.length > 0) {
            formatted += `\nNearby Peers:\n`;
            esp2Data.payload.nearby_peers.forEach(peer => {
              formatted += `  ${peer.device_id} (${peer.rssi}dBm)`;
              
              // Calculate distance from RSSI (monitor does the calculation)
              if (peer.rssi && peer.rssi < 0) {
                const distance = calculateDistance(peer.rssi);
                const confidence = calculateConfidence(peer.rssi);
                formatted += ` | 📏 ${distance.toFixed(1)}m (conf: ${(confidence * 100).toFixed(0)}%)`;
              }
              formatted += `\n`;
            });
          }
        }
      }
      
      return `<pre class="clean-message">${formatted}</pre>`;
    } catch (e) {
      return `ESP1 Gateway: ${esp2Type.toUpperCase()} from ${esp2Device}\nRaw data: ${data.esp2_raw_data}`;
    }
  } else {
    // Gateway status message only
    let formatted = `🔧 ESP1 Gateway: STATUS from Gateway\n`;
    formatted += `⏱️ Uptime: ${data.uptime}s\n`;
    formatted += `📈 Total: ${data.message_stats?.total || 0}\n`;
    formatted += `📄 Last: ${data.gateway_health?.last_message_type || 'none'}\n`;
    
    return `<pre class="clean-message">${formatted}</pre>`;
  }
}

function formatESP2Message(data) {
  const messageType = data.message_type || 'unknown';
  const deviceId = data.source_device?.device_id || data.device_id || 'Unknown';
  const version = data.version || 'Unknown';
  
  const messageIcon = getMessageTypeIcon(messageType);
  let formatted = `${messageIcon} ESP2 ${messageType.toUpperCase()}: from ${deviceId}\n`;
  formatted += `Version: ${version}    `;
  formatted += `Time: ${data.timestamp || 'Unknown'}\n`;
  
  // Add basic payload info
  if (data.payload) {
    if (data.payload.free_heap) {
      formatted += `Free Heap: ${data.payload.free_heap}    `;
    }
    if (data.payload.peer_count !== undefined) {
      formatted += `Peers: ${data.payload.peer_count}`;
    }
  }
  
  return `<pre class="clean-message">${formatted}</pre>`;
}

function addLogEntry(message, source = 'SYSTEM', isESPNowRelay = false, data = null) {
  const logEntry = document.createElement('div');
  
  // Detect message type for color coding
  let messageTypeClass = '';
  if (data && typeof data === 'object') {
    // Direct ESP2 message type
    if (data.message_type) {
      messageTypeClass = data.message_type.toLowerCase();
    }
    // ESP1 Gateway relayed message type
    else if (data.esp2_message_type) {
      messageTypeClass = data.esp2_message_type.toLowerCase();
    }
    // Try to parse from esp2_raw_data
    else if (data.esp2_raw_data) {
      try {
        const esp2Data = JSON.parse(data.esp2_raw_data);
        if (esp2Data.message_type) {
          messageTypeClass = esp2Data.message_type.toLowerCase();
        }
      } catch (e) {
        // Ignore parsing errors
      }
    }
  }
  
  // Combine source and message type classes
  const classNames = [`log-entry`, source.toLowerCase()];
  if (messageTypeClass) {
    classNames.push(messageTypeClass);
  }
  logEntry.className = classNames.join(' ');
  
  const timestamp = new Date().toLocaleTimeString();
  
  // Format message based on data type
  let formattedMessage = message;
  
  // If this is JSON data, format it nicely
  if (data && typeof data === 'object' && data !== null) {
    formattedMessage = formatMessageData(data, source);
  }
  
  logEntry.innerHTML = `
    <span class="log-timestamp">[${timestamp}]</span>
    <span class="log-source">${source}</span>
    <span class="log-message">${formattedMessage}</span>
  `;
  
  if (shouldShowLogEntry(source)) {
    logPanel.appendChild(logEntry);
    
    if (autoScroll) {
      logPanel.scrollTop = logPanel.scrollHeight;
    }
  }
  
  // Update message statistics
  messageCount++;
  lastMessageTime = Date.now();
  
  // Update network analysis if WiFi data
  if (data && data.rssi) {
    updateSignalBars(data.rssi);
    if (wifiSignalDetail) {
      wifiSignalDetail.textContent = `${data.rssi} dBm`;
    }
    if (wifiDistanceDetail && data.distance) {
      wifiDistanceDetail.textContent = `${data.distance} m`;
    }
  }
}

function shouldShowLogEntry(source) {
  if (logFilter === 'ALL') return true;
  return source.toUpperCase().includes(logFilter);
}

function filterLogEntries() {
  const entries = logPanel.querySelectorAll('.log-entry');
  entries.forEach(entry => {
    const source = entry.querySelector('.log-source').textContent;
    entry.style.display = shouldShowLogEntry(source) ? 'flex' : 'none';
  });
}

function updateConnectionStatus(status) {
  // Update status indicators
  updateStatusIndicator(wifiStatus, status.wifi?.connected);
  updateStatusIndicator(relayUsbStatus, status.relayUsb?.connected);
  updateStatusIndicator(relayWifiStatus, status.relayWifi?.connected);
  updateStatusIndicator(usbStatus, status.usb?.connected);
  
  // Update connection state
  isConnected = status.usb?.connected || false;
  
  // Update UI based on connection state
  if (isConnected) {
    connectBtn.style.display = 'none';
    disconnectBtn.style.display = 'inline-block';
  } else {
    connectBtn.style.display = 'inline-block';
    disconnectBtn.style.display = 'none';
  }
}

function updateStatusIndicator(indicator, connected) {
  if (!indicator) return;
  
  indicator.className = 'status-indicator';
  if (connected) {
    indicator.classList.add('connected');
  } else {
    indicator.classList.add('disconnected');
  }
}

// IPC Event listeners
window.electronAPI.onLog((logData) => {
  addLogEntry(logData.message, logData.source, logData.isESPNowRelay, logData.data);
  addActivityItem(logData.message, logData.source);
});

window.electronAPI.onConnectionStatus((status) => {
  connectionStatus = status;
  updateConnectionStatus(status);
});

window.electronAPI.onGatewayStats((stats) => {
  gatewayStats = stats;
  updateDashboard();
});