function resolveDeviceKey({ client, topic, deviceModel }) {
  const topicDeviceKey = topic?.split('/')[1];
  if (topicDeviceKey && deviceModel.hasDevice(topicDeviceKey)) {
    return topicDeviceKey;
  }

  const mappedDeviceKey = client?.id ? deviceModel.getDeviceByClient(client.id) : null;
  if (mappedDeviceKey && deviceModel.hasDevice(mappedDeviceKey)) {
    return mappedDeviceKey;
  }

  if (client?.id && deviceModel.hasDevice(client.id)) {
    return client.id;
  }

  return null;
}

function publishPresence(aedes, deviceKey, presence) {
  [`home/${deviceKey}/status`, `home/${deviceKey}`].forEach((topic) => {
    aedes.publish(
      {
        topic,
        payload: presence,
        qos: 0,
        retain: true
      },
      () => {}
    );
  });
}

function registerMqttHandlers({ aedes, io, deviceModel }) {
  aedes.on('clientReady', (client) => {
    const deviceKey = resolveDeviceKey({ client, deviceModel });
    if (!deviceKey) return;

    deviceModel.mapClientToDevice(client.id, deviceKey);
    deviceModel.updateSignal(deviceKey, '已連線');
    console.log(`🟢 [MQTT] ${deviceKey} 已連線`);
    publishPresence(aedes, deviceKey, 'online');
    io.emit('updateSignals', deviceModel.getSignals());
  });

  aedes.on('publish', (packet, client) => {
    if (!client) return;

    const topic = packet.topic;
    const payload = packet.payload.toString();
    const deviceKey = resolveDeviceKey({ client, topic, deviceModel });

    if (!deviceKey) return;

    const previousDeviceKey = deviceModel.getDeviceByClient(client.id);
    const previousSignal = String(deviceModel.getSignal(deviceKey) || '').trim().toLowerCase();

    deviceModel.mapClientToDevice(client.id, deviceKey);

    if (previousDeviceKey !== deviceKey || previousSignal === '離線' || previousSignal === 'offline') {
      console.log(`🟢 [MQTT] ${deviceKey} 已連線`);
      publishPresence(aedes, deviceKey, 'online');
    }

    deviceModel.updateSignal(deviceKey, payload);
    io.emit('updateSignals', deviceModel.getSignals());
  });

  aedes.on('clientDisconnect', (client) => {
    const deviceKey = resolveDeviceKey({ client, deviceModel });
    if (!deviceKey) return;

    deviceModel.updateSignal(deviceKey, '離線');
    deviceModel.removeClient(client.id);
    console.log(`📴 [MQTT] ${deviceKey} 已離線`);
    publishPresence(aedes, deviceKey, 'offline');
    io.emit('updateSignals', deviceModel.getSignals());
  });
}

module.exports = {
  registerMqttHandlers
};
