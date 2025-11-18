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
    // ESP-NOW relay message - just show raw JSON with highlighting
    if (logEntry.isESPNowRelay || (logEntry.message && logEntry.message.includes('sender_mac'))) {
      // Just display the raw message as-is
      messageSpan.textContent = logEntry.message;
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
