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
  
  // ESP2 Device Registry & Distance Tracking
  getESP2Devices: () => ipcRenderer.invoke('get-esp2-devices'),
  getAllDistances: () => ipcRenderer.invoke('get-all-distances'),
  getTriangulationStatus: () => ipcRenderer.invoke('get-triangulation-status'),
  getDistanceHistory: () => ipcRenderer.invoke('get-distance-history'),
  clearDeviceRegistry: () => ipcRenderer.invoke('clear-device-registry'),
  
  onLog: (callback) => ipcRenderer.on('log', (event, data) => callback(data)),
  onConnectionStatus: (callback) => ipcRenderer.on('connection-status', (event, data) => callback(data)),
  onGatewayStats: (callback) => ipcRenderer.on('gateway-stats', (event, data) => callback(data))
});
