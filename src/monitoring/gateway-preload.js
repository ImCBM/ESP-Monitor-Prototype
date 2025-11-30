const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // Gateway monitoring specific APIs
  getGatewayStats: () => ipcRenderer.invoke('get-gateway-stats'),
  resetGatewayStats: () => ipcRenderer.invoke('reset-gateway-stats'),
  
  // Listen for real-time updates
  onGatewayStats: (callback) => ipcRenderer.on('gateway-stats', (event, data) => callback(data)),
  onConnectionStatus: (callback) => ipcRenderer.on('connection-status', (event, data) => callback(data)),
  onLog: (callback) => ipcRenderer.on('log', (event, data) => callback(data)),
  
  // Remove listeners
  removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
});