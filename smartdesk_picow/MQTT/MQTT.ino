#include <WiFi.h>
#include <PubSubClient.h>

// 1. 修改為你的網路資訊
const char* ssid = "iSpan-R309";
const char* password = "66316588";
const char* mqtt_server = "192.168.39.107"; // 你的電腦 IP 位址

// 2. 定義 SmartDesk 專用的主題
const char* topic_pub = "home/smartDesk";         // 回報給 Server 的狀態
const char* topic_sub = "home/smartDesk/control"; // 接收來自網頁的指令

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
int counter = 0;

void setup_wifi() {
  Serial.print("\n正在連接 WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已連線！ IP 位址: ");
  Serial.println(WiFi.localIP());
}

// 接收來自網頁指令的處理函式
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("📩 [SmartDesk 收到指令]: ");
  Serial.println(message);

  // 邏輯測試：控制板載 LED
  if (message == "on") {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(">>> 桌面燈光/設備 已開啟");
  } else if (message == "off") {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(">>> 桌面燈光/設備 已關閉");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("嘗試 MQTT 連線...");
    // 建立唯一的 Client ID
    String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("✅ 連線成功");
      client.subscribe(topic_sub); // 務必重新訂閱
    } else {
      Serial.print("❌ 失敗, 錯誤碼=");
      Serial.print(client.state());
      Serial.println(" 2秒後重試");
      delay(2000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 每 5 秒更新一次桌面狀態給網頁
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    counter++;
    
    // 模擬傳送桌面數據（例如環境亮度或使用時間）
    String status = "桌面使用中 - 序號:" + String(counter);
    client.publish(topic_pub, status.c_str());
    
    Serial.print("📤 [SmartDesk 狀態回報]: ");
    Serial.println(status);
  }
}

