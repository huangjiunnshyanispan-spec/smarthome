function registerSocketHandlers({ io, aedes, deviceModel }) {
  io.on('connection', (socket) => {
    socket.emit('updateSignals', deviceModel.getSignals());

    socket.on('webControl', (data) => {
      const { device, cmd, text } = data;
      let mqttTopic;
      let payload;

      if (device === 'microphone' && cmd === 'say') {
        mqttTopic = 'home/speaker/say';
        payload = text;
      } else {
        mqttTopic = `home/${device}/control`;
        payload = cmd;
      }

      aedes.publish(
        {
          topic: mqttTopic,
          payload: payload,
          qos: 0,
          retain: false
        },
        () => {
          console.log(`📤 [Web -> MQTT] 指令發送至 ${mqttTopic}: ${payload}`);
        }
      );
    });
  });
}

module.exports = {
  registerSocketHandlers
};
