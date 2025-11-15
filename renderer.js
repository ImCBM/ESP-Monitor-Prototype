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
const relayStatus = document.getElementById('relay-status');
const usbStatus = document.getElementById('usb-status');
const wifiSignal = document.getElementById('wifi-signal');

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
  } else {
    wifiStatus.classList.remove('connected');
    wifiSignal.textContent = '';
  }

  // Relay status
  if (status.relay.connected) {
    relayStatus.classList.add('connected');
  } else {
    relayStatus.classList.remove('connected');
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
  const logEntry = {
    timestamp,
    source,
    message,
    data
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
  return logEntry.source === currentFilter;
}

// Render a single log entry
function renderLogEntry(logEntry) {
  const entry = document.createElement('div');
  entry.className = `log-entry ${logEntry.source.toLowerCase()}`;

  const timestampSpan = document.createElement('span');
  timestampSpan.className = 'log-timestamp';
  timestampSpan.textContent = `[${logEntry.timestamp}]`;

  const sourceSpan = document.createElement('span');
  sourceSpan.className = `log-source ${logEntry.source.toLowerCase()}`;
  sourceSpan.textContent = logEntry.source;

  const messageSpan = document.createElement('span');
  messageSpan.className = 'log-message';
  messageSpan.textContent = logEntry.message;

  entry.appendChild(timestampSpan);
  entry.appendChild(sourceSpan);
  entry.appendChild(messageSpan);

  // Show formatted data if available
  if (logEntry.data && Object.keys(logEntry.data).length > 1) {
    const dataDiv = document.createElement('div');
    dataDiv.className = 'log-data';
    dataDiv.textContent = JSON.stringify(logEntry.data, null, 2);
    messageSpan.appendChild(dataDiv);
  }

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
