const express = require('express');
const app = express();
const http = require('http');
const net = require('net');
const path = require('path');
const aedes = require('aedes')();
const socketIo = require('socket.io');
const deviceModel = require('./models/deviceModel');
const { registerMqttHandlers } = require('./controllers/mqttController');
const { registerSocketHandlers } = require('./controllers/socketController');
const { createPageController } = require('./controllers/pageController');
const { createDeviceRouter } = require('./routes/deviceRoutes');

const webPort = 3000;
const mqttPort = 1883;

// 建立伺服器架構
const server = http.createServer(app);
const io = socketIo(server);
const mqttServer = net.createServer(aedes.handle);

// Middleware 設定
app.use(express.static(path.join(__dirname, 'public'))); // 存放 index.html
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));
const pageController = createPageController({ deviceModel });
const deviceRouter = createDeviceRouter({ pageController });

app.use('/', deviceRouter);

// ==========================================
// 1. MQTT Broker 邏輯 (與 Pico W 溝通)
// ==========================================
mqttServer.listen(mqttPort, () => {
  console.log(`[MQTT] Broker 運行於埠號: ${mqttPort}`);
});

registerMqttHandlers({ aedes, io, deviceModel });

// ==========================================
// 2. Socket.io 邏輯 (與 網頁瀏覽器 溝通)
// ==========================================
registerSocketHandlers({ io, aedes, deviceModel });

server.listen(webPort, () => {
  console.log(`🖥️  網頁儀表板已啟動: http://localhost:${webPort}`);
});
