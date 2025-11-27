const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  listSerialPorts: () => ipcRenderer.invoke('list-serial-ports'),
  openSerialPort: (portPath) => ipcRenderer.invoke('open-serial-port', portPath),
  closeSerialPort: () => ipcRenderer.invoke('close-serial-port'),
  getConnectionStatus: () => ipcRenderer.invoke('get-connection-status'),
  clearLog: () => ipcRenderer.invoke('clear-log'),
  
  // Gateway monitoring
  getGatewayStats: () => ipcRenderer.invoke('get-gateway-stats'),
  resetGatewayStats: () => ipcRenderer.invoke('reset-gateway-stats'),
  
  onLog: (callback) => ipcRenderer.on('log', (event, data) => callback(data)),
  onConnectionStatus: (callback) => ipcRenderer.on('connection-status', (event, data) => callback(data)),
  onGatewayStats: (callback) => ipcRenderer.on('gateway-stats', (event, data) => callback(data))
});
