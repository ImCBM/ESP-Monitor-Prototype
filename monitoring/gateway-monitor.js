// ESP Gateway Monitor JavaScript - Electron Integration
class GatewayMonitor {
    constructor() {
        this.messageCount = 0;
        this.isPaused = false;
        this.esp2Devices = new Map();
        this.messageStats = {
            ping: 0,
            handshake: 0,
            data: 0,
            triangulation: 0,
            relay: 0,
            wifiScan: 0,
            optimization: 0,
            unknown: 0,
            total: 0,
            delivered: 0
        };
        
        this.gatewayInfo = {
            deviceId: 'Unknown',
            version: 'Unknown',
            uptime: 0,
            protocolRange: 'Unknown',
            usbStatus: 'Disconnected',
            espNowStatus: 'Unknown',
            macAddress: 'Unknown',
            protocolMismatches: 0,
            lastSender: 'None',
            lastMessageType: 'None'
        };

        this.connections = {
            wifi: { connected: false },
            relayUsb: { connected: false },
            relayWifi: { connected: false },
            usb: { connected: false }
        };

        this.initializeEventListeners();
        this.initializeElectronAPI();
        this.startUpdateLoop();
    }

    initializeEventListeners() {
        // Control buttons
        document.getElementById('clear-messages').addEventListener('click', () => {
            this.clearMessages();
        });

        document.getElementById('pause-messages').addEventListener('click', () => {
            this.togglePause();
        });

        // Window visibility handling
        document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'visible') {
                this.refreshDisplay();
            }
        });
    }

    initializeElectronAPI() {
        if (typeof window.electronAPI === 'undefined') {
            console.error('Electron API not available');
            this.showError('This application requires Electron to function properly.');
            return;
        }

        console.log('🔌 Connecting to Electron main process...');
        
        // Listen for gateway statistics updates
        window.electronAPI.onGatewayStats((stats) => {
            this.updateGatewayStats(stats);
        });

        // Listen for connection status updates  
        window.electronAPI.onConnectionStatus((connections) => {
            this.updateConnectionStatus(connections);
        });

        // Listen for real-time log messages
        window.electronAPI.onLog((logData) => {
            this.processLogMessage(logData);
        });

        // Load initial data
        this.loadInitialData();
    }

    async loadInitialData() {
        try {
            const stats = await window.electronAPI.getGatewayStats();
            this.updateGatewayStats(stats);
            console.log('✅ Initial gateway stats loaded');
        } catch (error) {
            console.error('Failed to load initial gateway stats:', error);
            this.showError('Failed to load gateway statistics');
        }
    }

    updateGatewayStats(stats) {
        if (!stats) return;

        // Update gateway info
        Object.assign(this.gatewayInfo, stats.gatewayInfo);
        
        // Update message statistics
        Object.assign(this.messageStats, stats.messageStats);
        
        // Update ESP2 device count
        this.esp2Devices = new Map();
        for (let i = 1; i <= stats.esp2DeviceCount; i++) {
            this.esp2Devices.set(`ESP2_SENSOR_${i.toString().padStart(2, '0')}`, {
                deviceId: `ESP2_SENSOR_${i.toString().padStart(2, '0')}`,
                lastSeen: new Date().toISOString(),
                messageCount: Math.floor(Math.random() * 100),
                phases: new Set([1, 2, 3, 4, 5])
            });
        }

        this.updateGatewayDisplay();
        this.updateStatisticsDisplay();
        this.updateActivitySummary();
        this.updateNetworkTopology();
    }

    updateConnectionStatus(connections) {
        if (!connections) return;
        
        this.connections = connections;
        
        const gatewayStatus = document.getElementById('gateway-status');
        const esp2Network = document.getElementById('esp2-network');
        
        if (connections.usb.connected) {
            gatewayStatus.textContent = 'Online';
            gatewayStatus.className = 'status-indicator online';
            this.gatewayInfo.usbStatus = 'Connected';
        } else {
            gatewayStatus.textContent = 'Offline';
            gatewayStatus.className = 'status-indicator offline';
            this.gatewayInfo.usbStatus = 'Disconnected';
        }

        if (connections.relayUsb.connected) {
            esp2Network.textContent = `Active (${this.esp2Devices.size} devices)`;
            esp2Network.className = 'status-indicator online';
            this.gatewayInfo.espNowStatus = 'Active';
        } else {
            esp2Network.textContent = 'No Activity';
            esp2Network.className = 'status-indicator offline';
            this.gatewayInfo.espNowStatus = 'Inactive';
        }

        this.updateGatewayDisplay();
    }

    processLogMessage(logData) {
        if (this.isPaused || !logData) return;

        // Only process ESP2 messages or ESP1 gateway status
        if (logData.source === 'RELAY_USB' || logData.source === 'ESP1_GATEWAY') {
            this.addMessageToLog(logData);
        }
    }

    showError(message) {
        const messageLog = document.getElementById('message-log');
        messageLog.innerHTML = `<div class="error-message" style="color: #dc3545; text-align: center; padding: 20px;">${message}</div>`;
    }

    addMessageToLog(logData) {
        if (this.isPaused) return;

        const messageLog = document.getElementById('message-log');
        const placeholder = messageLog.querySelector('.log-placeholder');
        if (placeholder) {
            placeholder.remove();
        }

        const logEntry = document.createElement('div');
        
        // Determine phase and message type from log data
        let phase = 1;
        let messageType = 'unknown';
        let deviceId = 'Unknown';
        
        if (logData.data) {
            messageType = logData.data.message_type || 'unknown';
            deviceId = logData.data.source_device?.device_id || 'Unknown';
            
            // Determine phase from message type
            const phaseMap = {
                'ping': 1, 'data': 1,
                'wifi_scan': 2,
                'handshake': 3,
                'triangulation': 4,
                'relay': 5,
                'optimization': 6
            };
            phase = phaseMap[messageType] || 1;
        }
        
        logEntry.className = `log-entry phase-${phase}`;

        const formattedTime = new Date(logData.timestamp).toLocaleTimeString();

        logEntry.innerHTML = `
            <div class="log-header">
                <span class="log-type">Phase ${phase} - ${messageType.toUpperCase()}</span>
                <span class="log-timestamp">${formattedTime}</span>
            </div>
            <div class="log-content">
                From: ${deviceId}<br>
                Source: ${logData.source}<br>
                Data: ${JSON.stringify(logData.data || {}).substring(0, 100)}...
            </div>
        `;

        messageLog.insertBefore(logEntry, messageLog.firstChild);

        // Limit log entries to prevent memory issues
        const entries = messageLog.querySelectorAll('.log-entry');
        if (entries.length > 100) {
            entries[entries.length - 1].remove();
        }

        // Auto-scroll to top for new messages
        messageLog.scrollTop = 0;
    }

    updateConnectionStatus(status) {
        // This method is now handled by updateConnectionStatus from Electron data
        console.log('Connection status updated:', status);
    }

    updateGatewayDisplay() {
        document.getElementById('gateway-id').textContent = this.gatewayInfo.deviceId;
        document.getElementById('gateway-version').textContent = this.gatewayInfo.version;
        document.getElementById('protocol-range').textContent = this.gatewayInfo.protocolRange;
        document.getElementById('usb-status').textContent = this.gatewayInfo.usbStatus;
        document.getElementById('espnow-status').textContent = this.gatewayInfo.espNowStatus;
        document.getElementById('esp-mac').textContent = this.gatewayInfo.macAddress;
        document.getElementById('protocol-mismatches').textContent = this.gatewayInfo.protocolMismatches;
    }

    updateStatisticsDisplay() {
        document.getElementById('ping-count').textContent = this.messageStats.ping;
        document.getElementById('data-count').textContent = this.messageStats.data;
        document.getElementById('wifi-scan-count').textContent = this.messageStats.wifiScan;
        document.getElementById('handshake-count').textContent = this.messageStats.handshake;
        document.getElementById('triangulation-count').textContent = this.messageStats.triangulation;
        document.getElementById('relay-count').textContent = this.messageStats.relay;
        document.getElementById('optimization-count').textContent = this.messageStats.optimization;
    }

    updateActivitySummary() {
        document.getElementById('total-messages').textContent = this.messageStats.total;
        document.getElementById('delivered-messages').textContent = this.messageStats.delivered;
        document.getElementById('last-sender').textContent = this.gatewayInfo.lastSender;
        document.getElementById('last-message-type').textContent = this.gatewayInfo.lastMessageType;
    }

    updateNetworkTopology() {
        const canvas = document.getElementById('topology-canvas');
        const placeholder = canvas.querySelector('.topology-placeholder');
        
        if (this.esp2Devices.size > 0 && placeholder) {
            placeholder.remove();
        }

        // Simple topology visualization
        // Position ESP1 gateway at center
        if (!canvas.querySelector('.esp1-node')) {
            const esp1Node = document.createElement('div');
            esp1Node.className = 'esp-node esp1';
            esp1Node.textContent = 'ESP1';
            esp1Node.style.left = '50%';
            esp1Node.style.top = '50%';
            esp1Node.style.transform = 'translate(-50%, -50%)';
            esp1Node.title = 'ESP1 Wired Gateway';
            canvas.appendChild(esp1Node);
        }

        // Position ESP2 devices around ESP1
        let angle = 0;
        const radius = 100;
        this.esp2Devices.forEach((device, deviceId) => {
            let node = canvas.querySelector(`[data-device="${deviceId}"]`);
            if (!node) {
                node = document.createElement('div');
                node.className = 'esp-node esp2';
                node.textContent = deviceId.replace('ESP2_SENSOR_', 'S');
                node.setAttribute('data-device', deviceId);
                node.title = `${deviceId}\nMessages: ${device.messageCount}\nPhases: ${Array.from(device.phases).join(', ')}`;
                
                const x = 50 + (radius * Math.cos(angle * Math.PI / 180)) / canvas.offsetWidth * 100;
                const y = 50 + (radius * Math.sin(angle * Math.PI / 180)) / canvas.offsetHeight * 100;
                node.style.left = x + '%';
                node.style.top = y + '%';
                node.style.transform = 'translate(-50%, -50%)';
                
                canvas.appendChild(node);
                angle += 360 / Math.max(this.esp2Devices.size, 6);
            }
        });
    }

    clearMessages() {
        const messageLog = document.getElementById('message-log');
        messageLog.innerHTML = '<div class="log-placeholder">📡 Message log cleared</div>';
    }

    togglePause() {
        this.isPaused = !this.isPaused;
        const button = document.getElementById('pause-messages');
        button.textContent = this.isPaused ? 'Resume' : 'Pause';
        button.style.background = this.isPaused ? '#ffc107' : '';
    }

    startUpdateLoop() {
        setInterval(() => {
            // Update uptime
            this.gatewayInfo.uptime++;
            const uptimeElement = document.getElementById('gateway-uptime');
            if (uptimeElement) {
                const hours = Math.floor(this.gatewayInfo.uptime / 3600);
                const minutes = Math.floor((this.gatewayInfo.uptime % 3600) / 60);
                const seconds = this.gatewayInfo.uptime % 60;
                uptimeElement.textContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
            }

            // Update ESP2 network status
            const esp2Network = document.getElementById('esp2-network');
            if (esp2Network && this.esp2Devices.size > 0) {
                esp2Network.textContent = `Active (${this.esp2Devices.size} devices)`;
            }
        }, 1000);
    }

    refreshDisplay() {
        this.updateGatewayDisplay();
        this.updateStatisticsDisplay();
        this.updateActivitySummary();
        this.updateNetworkTopology();
    }
}

// Initialize monitor when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.gatewayMonitor = new GatewayMonitor();
    console.log('🚀 ESP Gateway Monitor initialized for Electron');
});

// Handle window resize for topology canvas
window.addEventListener('resize', () => {
    if (window.gatewayMonitor) {
        window.gatewayMonitor.updateNetworkTopology();
    }
});

// Cleanup listeners when window is closed
window.addEventListener('beforeunload', () => {
    if (window.electronAPI && window.electronAPI.removeAllListeners) {
        window.electronAPI.removeAllListeners('gateway-stats');
        window.electronAPI.removeAllListeners('connection-status');
        window.electronAPI.removeAllListeners('log');
    }
});