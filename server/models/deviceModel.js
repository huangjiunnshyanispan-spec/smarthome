const { INITIAL_DEVICE_SIGNALS } = require('../config/deviceConfig');

class DeviceModel {
  constructor() {
    this.deviceSignals = { ...INITIAL_DEVICE_SIGNALS };
    this.clientToDevice = {};
  }

  hasDevice(deviceKey) {
    return Object.prototype.hasOwnProperty.call(this.deviceSignals, deviceKey);
  }

  getSignals() {
    return { ...this.deviceSignals };
  }

  getSignal(deviceKey) {
    return this.deviceSignals[deviceKey] || null;
  }

  updateSignal(deviceKey, signal) {
    if (!this.hasDevice(deviceKey)) return false;
    this.deviceSignals[deviceKey] = signal;
    return true;
  }

  mapClientToDevice(clientId, deviceKey) {
    this.clientToDevice[clientId] = deviceKey;
  }

  getDeviceByClient(clientId) {
    return this.clientToDevice[clientId];
  }

  removeClient(clientId) {
    delete this.clientToDevice[clientId];
  }
}

module.exports = new DeviceModel();
