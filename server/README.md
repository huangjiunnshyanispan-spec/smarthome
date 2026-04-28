# 智慧家居伺服器 (Socket.IO 版本)

這是一個使用 Socket.IO 實現實時通信的智慧家居伺服器。

## 功能特點
- 實時雙向通信
- 自動更新裝置狀態
- 支援多個智慧家居裝置

## 運行方式
```bash
npm install
node server.js
```

## Socket.IO 事件

### 伺服器事件 (發送給客戶端)
- `signals`: 發送所有裝置的最新狀態
- `status`: 回應特定裝置的狀態查詢
- `error`: 錯誤訊息

### 客戶端事件 (發送給伺服器)
- `signal`: 裝置發送訊號
  ```javascript
  socket.emit('signal', {
    device: 'smartDesk',
    signalData: { message: '桌面上有物品' }
  });
  ```
- `getStatus`: 請求特定裝置狀態
  ```javascript
  socket.emit('getStatus', 'smartDesk');
  ```

## 裝置端使用範例
參見 `device-example.js`

## 支援裝置
- smartDesk: 智慧桌面助理
- petFeeder: 寵物餵食機
- voiceRecognition: 語音辨識系統
- surveillance: 影像監控系統
- smartGarage: 智慧車庫

## 網頁介面

訪問主頁面查看所有裝置狀態，點擊進入各裝置詳情頁面。

網頁採用深色主題，優化手機介面。