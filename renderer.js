// DOM Elements
const logPanel = document.getElementById('log-panel');
const serialPortSelect = document.getElementById('serial-port-select');
const connectBtn = document.getElementById('connect-btn');
const disconnectBtn = document.getElementById('disconnect-btn');
const refreshBtn = document.getElementById('refresh-btn');
const clearBtn = document.getElementById('clear-btn');
const autoScrollCheckbox = document.getElementById('auto-scroll');
const filterSelect = document.getElementById('filter-select');

// Status indicators
const wifiStatus = document.getElementById('wifi-status');
const relayUsbStatus = document.getElementById('relay-usb-status');
const relayWifiStatus = document.getElementById('relay-wifi-status');
const usbStatus = document.getElementById('usb-status');
const wifiSignal = document.getElementById('wifi-signal');
const wifiDistance = document.getElementById('wifi-distance');

// State
let currentFilter = 'ALL';
let autoScroll = true;
let allLogs = [];

// Initialize
async function init() {
  await loadSerialPorts();
  setupEventListeners();
  loadConnectionStatus();
}

// Load available serial ports
async function loadSerialPorts() {
  try {
    const ports = await window.electronAPI.listSerialPorts();
    serialPortSelect.innerHTML = '<option value="">Select Serial Port...</option>';
    
    ports.forEach(port => {
      const option = document.createElement('option');
      option.value = port.path;
      option.textContent = `${port.path}${port.manufacturer ? ` (${port.manufacturer})` : ''}`;
      serialPortSelect.appendChild(option);
    });
  } catch (error) {
    addLogEntry('ERROR', 'Failed to load serial ports: ' + error.message);
  }
}

// Setup event listeners
function setupEventListeners() {
  connectBtn.addEventListener('click', async () => {
    const portPath = serialPortSelect.value;
    if (!portPath) {
      alert('Please select a serial port');
      return;
    }
    
    try {
      await window.electronAPI.openSerialPort(portPath);
      connectBtn.style.display = 'none';
      disconnectBtn.style.display = 'inline-block';
      serialPortSelect.disabled = true;
    } catch (error) {
      addLogEntry('ERROR', 'Failed to connect: ' + error.message);
    }
  });

  disconnectBtn.addEventListener('click', async () => {
    try {
      await window.electronAPI.closeSerialPort();
      connectBtn.style.display = 'inline-block';
      disconnectBtn.style.display = 'none';
      serialPortSelect.disabled = false;
    } catch (error) {
      addLogEntry('ERROR', 'Failed to disconnect: ' + error.message);
    }
  });

  refreshBtn.addEventListener('click', async () => {
    await loadSerialPorts();
    addLogEntry('SYSTEM', 'Serial ports refreshed');
  });

  clearBtn.addEventListener('click', () => {
    allLogs = [];
    logPanel.innerHTML = '';
    addLogEntry('SYSTEM', 'Log cleared');
  });

  autoScrollCheckbox.addEventListener('change', (e) => {
    autoScroll = e.target.checked;
  });

  filterSelect.addEventListener('change', (e) => {
    currentFilter = e.target.value;
    applyFilter();
  });

  // Listen for log messages from main process
  window.electronAPI.onLog((data) => {
    addLogEntry(data.source, data.message, data.data);
  });

  // Listen for connection status updates
  window.electronAPI.onConnectionStatus((status) => {
    updateConnectionStatus(status);
  });
}

// Load initial connection status
async function loadConnectionStatus() {
  try {
    const status = await window.electronAPI.getConnectionStatus();
    updateConnectionStatus(status);
  } catch (error) {
    console.error('Failed to load connection status:', error);
  }
}

// Update connection status indicators
function updateConnectionStatus(status) {
  // WiFi status
  if (status.wifi.connected) {
    wifiStatus.classList.add('connected');
    if (status.wifi.signalStrength) {
      wifiSignal.textContent = `(${status.wifi.signalStrength} dBm)`;
    }
    if (status.wifi.distance > 0) {
      wifiDistance.textContent = `~${status.wifi.distance}m`;
    } else {
      wifiDistance.textContent = '';
    }
  } else {
    wifiStatus.classList.remove('connected');
    wifiSignal.textContent = '';
    wifiDistance.textContent = '';
  }

  // Relay (USB) status
  if (status.relayUsb && status.relayUsb.connected) {
    console.log('Setting USB relay indicator to GREEN');
    relayUsbStatus.classList.add('connected');
  } else {
    relayUsbStatus.classList.remove('connected');
  }

  // Relay (WiFi) status
  if (status.relayWifi && status.relayWifi.connected) {
    console.log('Setting WiFi relay indicator to GREEN');
    relayWifiStatus.classList.add('connected');
  } else {
    relayWifiStatus.classList.remove('connected');
  }

  // USB status
  if (status.usb.connected) {
    usbStatus.classList.add('connected');
  } else {
    usbStatus.classList.remove('connected');
  }
}

// Add log entry
function addLogEntry(source, message, data = null) {
  const timestamp = new Date().toLocaleTimeString();
  
  // Detect relay messages by source or content
  const isESPNowRelay = data?.isESPNowRelay || 
                        source === 'RELAY_USB' || 
                        source === 'RELAY_WIFI' ||
                        (data?.sender_mac && (data?.received_data || data?.relayed_data)) ||
                        (data?.message && data?.message.includes('ESP-NOW message')) ||
                        (message && message.includes('sender_mac'));
  
  const logEntry = {
    timestamp,
    source: source || 'UNKNOWN',
    message,
    data,
    mode: data?.mode,
    isESPNowRelay
  };

  allLogs.push(logEntry);

  if (shouldShowLog(logEntry)) {
    renderLogEntry(logEntry);
  }

  if (autoScroll) {
    logPanel.scrollTop = logPanel.scrollHeight;
  }
}

// Check if log should be shown based on filter
function shouldShowLog(logEntry) {
  if (currentFilter === 'ALL') return true;
  if (currentFilter === 'RELAY') {
    // Only show relayed ESP-NOW messages
    return logEntry.isESPNowRelay === true;
  }
  // Handle RELAY_USB and RELAY_WIFI as part of their respective categories
  if (currentFilter === 'USB' && logEntry.source === 'RELAY_USB') return true;
  if (currentFilter === 'WIFI' && logEntry.source === 'RELAY_WIFI') return true;
  
  return logEntry.source === currentFilter;
}

// Render a single log entry
function renderLogEntry(logEntry) {
  const entry = document.createElement('div');
  entry.className = `log-entry ${logEntry.source.toLowerCase()}`;
  
  // Highlight ESP-NOW relay messages
  if (logEntry.isESPNowRelay) {
    entry.classList.add('espnow-relay');
  }

  const timestampSpan = document.createElement('span');
  timestampSpan.className = 'log-timestamp';
  timestampSpan.textContent = `[${logEntry.timestamp}]`;

  const sourceSpan = document.createElement('span');
  sourceSpan.className = `log-source ${logEntry.source.toLowerCase()}`;
  
  // Show source with mode and ESP-NOW indicator
  let sourceText = logEntry.source;
  if (logEntry.isESPNowRelay) {
    if (logEntry.source === 'RELAY_USB') {
      sourceText = 'USB | RELAY';
    } else if (logEntry.source === 'RELAY_WIFI') {
      sourceText = 'WiFi | RELAY';
    } else {
      sourceText = 'ESP-NOW | RELAY';
    }
  }
  if (logEntry.mode && !logEntry.isESPNowRelay) {
    sourceText += ` (${logEntry.mode})`;
  }
  sourceSpan.textContent = sourceText;

  const messageSpan = document.createElement('span');
  messageSpan.className = 'log-message';
  
  // Parse and format the message for better readability
  if (logEntry.data) {
    // ESP-NOW relay message
    if (logEntry.data.sender_mac || logEntry.data.relayed_data || logEntry.data.received_data || 
        (logEntry.data.message && logEntry.data.message.includes('ESP-NOW'))) {
      
      // Try to extract data from various formats
      let senderMac = logEntry.data.sender_mac || 'Unknown';
      let espData = logEntry.data.relayed_data || logEntry.data.received_data || '';
      let deviceId = logEntry.data.device_id || 'Unknown Device';
      let messageCount = logEntry.data.message_count || 'N/A';
      let uptime = logEntry.data.uptime || 'N/A';
      let freeHeap = logEntry.data.free_heap || 'N/A';
      let receivedCount = logEntry.data.received_count || logEntry.data.receive_count || 'N/A';
      
      // If data is in raw format, try to parse it
      if (logEntry.data.raw && typeof logEntry.data.raw === 'string') {
        try {
          const rawData = JSON.parse(logEntry.data.raw);
          senderMac = rawData.sender_mac || senderMac;
          espData = rawData.received_data || rawData.relayed_data || espData;
          deviceId = rawData.device_id || deviceId;
          messageCount = rawData.message_count || messageCount;
          uptime = rawData.uptime || uptime;
          freeHeap = rawData.free_heap || freeHeap;
          receivedCount = rawData.received_count || rawData.receive_count || receivedCount;
        } catch (e) {
          // If parsing fails, extract from the message string
          const msg = logEntry.data.raw;
          const macMatch = msg.match(/"sender_mac":"([^"]+)"/);
          const dataMatch = msg.match(/"received_data":"([^"]+)"/);
          const deviceMatch = msg.match(/"device_id":"([^"]+)"/);
          const countMatch = msg.match(/"message_count":(\d+)/);
          const uptimeMatch = msg.match(/"uptime":(\d+)/);
          const heapMatch = msg.match(/"free_heap":(\d+)/);
          const receivedMatch = msg.match(/"receive_count":(\d+)/);
          
          if (macMatch) senderMac = macMatch[1];
          if (dataMatch) espData = dataMatch[1];
          if (deviceMatch) deviceId = deviceMatch[1];
          if (countMatch) messageCount = countMatch[1];
          if (uptimeMatch) uptime = uptimeMatch[1];
          if (heapMatch) freeHeap = heapMatch[1];
          if (receivedMatch) receivedCount = receivedMatch[1];
        }
      }
      
      messageSpan.innerHTML = `
        <strong>📡 ESP-NOW Relay Message</strong><br>
        <div style="margin: 8px 0; padding: 12px; background: rgba(70, 130, 180, 0.15); border: 1px solid rgba(70, 130, 180, 0.3); border-radius: 6px;">
          <div style="margin-bottom: 8px;">
            <span class="data-label">From Device:</span> <code style="background: rgba(70, 130, 180, 0.2); padding: 3px 8px; border-radius: 3px;">${deviceId}</code>
          </div>
          <div style="margin-bottom: 8px;">
            <span class="data-label">MAC Address:</span> <code style="background: rgba(70, 130, 180, 0.2); padding: 3px 8px; border-radius: 3px;">${senderMac}</code>
          </div>
          <div style="margin-bottom: 8px;">
            <span class="data-label">Message Content:</span><br>
            <code style="background: rgba(70, 130, 180, 0.2); padding: 6px 10px; border-radius: 3px; display: inline-block; margin-top: 4px; max-width: 100%; word-break: break-all;">${espData}</code>
          </div>
          <div style="font-size: 12px; color: #9cdcfe; border-top: 1px solid rgba(70, 130, 180, 0.2); padding-top: 8px; margin-top: 8px;">
            <strong>Device Statistics:</strong><br>
            Messages Sent: <code>${messageCount}</code> • 
            Messages Received: <code>${receivedCount}</code> • 
            Uptime: <code>${uptime}s</code> • 
            Free Memory: <code>${freeHeap} bytes</code>
          </div>
        </div>
      `;
    } 
    // Regular status message
    else if (logEntry.data.message) {
      messageSpan.innerHTML = `
        <strong>${logEntry.data.message}</strong><br>
        ${logEntry.data.received_count !== undefined ? `<span class="data-label">Received:</span> ${logEntry.data.received_count} | ` : ''}
        ${logEntry.data.uptime !== undefined ? `<span class="data-label">Uptime:</span> ${logEntry.data.uptime}s | ` : ''}
        ${logEntry.data.rssi !== undefined ? `<span class="data-label">RSSI:</span> ${logEntry.data.rssi} dBm` : ''}
      `;
    } else {
      messageSpan.textContent = logEntry.message;
    }
  } else {
    messageSpan.textContent = logEntry.message;
  }

  entry.appendChild(timestampSpan);
  entry.appendChild(sourceSpan);
  entry.appendChild(messageSpan);

  logPanel.appendChild(entry);
}

// Apply filter to existing logs
function applyFilter() {
  logPanel.innerHTML = '';
  allLogs.forEach(logEntry => {
    if (shouldShowLog(logEntry)) {
      renderLogEntry(logEntry);
    }
  });

  if (autoScroll) {
    logPanel.scrollTop = logPanel.scrollHeight;
  }
}

// Start the app
init();
