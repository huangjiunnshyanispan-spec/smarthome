#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// 設定區
// ==========================================
const char* ssid        = "iSpan-R309";
const char* password    = "66316588";
const char* mqtt_server = "192.168.39.107";

const char* topic_pub = "home/smartDesk";
const char* topic_sub = "home/smartDesk/control";

// ==========================================
// SSD1306
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// 設備定義
// ==========================================
const int DEVICE_COUNT = 5;

struct Device {
  const char* label;
  const char* topic;
  bool        online;
};

Device devices[DEVICE_COUNT] = {
  { "Desk   ", "home/smartDesk",        false },
  { "Feeder ", "home/petFeeder",        false },
  { "Voice  ", "home/voiceRecognition", false },
  { "Camera ", "home/surveillance",     false },
  { "Garage ", "home/smartGarage",      false }
};

// ==========================================
// 連線狀態 & MQTT
// ==========================================
bool wifiConnected = false;
bool mqttConnected = false;

WiFiClient   espClient;
PubSubClient client(espClient);

unsigned long lastMsg     = 0;
unsigned long lastDisplay = 0;
int counter = 0;

// ==========================================
// 畫面繪製函式
// ==========================================
void drawBootScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(8, 10);
  display.println("SmartDesk");
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println("Dashboard v1.0");
  display.setCursor(28, 52);
  display.println("Starting...");
  display.display();
}

void drawConnectingScreen(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
}

void drawDashboard() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // 第一列：伺服器連線狀態
  display.setCursor(0, 0);
  display.print("WiFi:");
  display.print(wifiConnected ? "OK" : "NG");
  display.print(" MQTT:");
  display.print(mqttConnected ? "OK" : "NG");

  // 設備狀態列（每行 10px）
  for (int i = 0; i < DEVICE_COUNT; i++) {
    int y = 12 + i * 10;
    display.setCursor(0, y);
    display.print(devices[i].label);
    display.print(devices[i].online ? "ON " : "OFF");
  }

  display.display();
}

// ==========================================
// MQTT Callback
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message  = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("📩 [");
  Serial.print(topicStr);
  Serial.print("] -> ");
  Serial.println(message);

  // 控制指令
  if (topicStr == String(topic_sub)) {
    if (message == "on")  digitalWrite(LED_BUILTIN, HIGH);
    if (message == "off") digitalWrite(LED_BUILTIN, LOW);
    return;
  }

  // 設備狀態更新
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (topicStr == String(devices[i].topic)) {
      bool wasOnline = devices[i].online;
      devices[i].online = (message != "offline");
      if (wasOnline != devices[i].online) {
        Serial.print(devices[i].online ? "✅ 上線: " : "📴 離線: ");
        Serial.println(devices[i].label);
      }
      return;
    }
  }
}

// ==========================================
// WiFi 連線
// ==========================================
void setup_wifi() {
  drawConnectingScreen("  Connecting WiFi...");
  Serial.print("連接 WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  wifiConnected = true;
  Serial.print("\n已連線! IP: ");
  Serial.println(WiFi.localIP());
}

// ==========================================
// MQTT 重連
// ==========================================
void reconnect() {
  mqttConnected = false;
  for (int i = 0; i < DEVICE_COUNT; i++) {
    devices[i].online = false;
  }

  drawConnectingScreen("  Connecting MQTT...");

  while (!client.connected()) {
    Serial.print("嘗試 MQTT 連線...");
    String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      mqttConnected = true;
      Serial.println("✅ 成功");

      client.subscribe(topic_sub);

      for (int i = 0; i < DEVICE_COUNT; i++) {
        client.subscribe(devices[i].topic);
        Serial.print("   訂閱: ");
        Serial.println(devices[i].topic);
      }
    } else {
      Serial.print("❌ 失敗(");
      Serial.print(client.state());
      Serial.println(") 2秒後重試");
      delay(2000);
    }
  }
}

// ==========================================
// Setup
// ==========================================
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("❌ SSD1306 初始化失敗，請檢查接線！");
    for (;;);
  }

  drawBootScreen();
  delay(2000);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ==========================================
// Loop
// ==========================================
void loop() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (!wifiConnected) {
    setup_wifi();
  }

  if (!client.connected()) {
    mqttConnected = false;
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  if (now - lastDisplay > 500) {
    lastDisplay = now;
    drawDashboard();
  }

  if (now - lastMsg > 5000) {
    lastMsg = now;
    counter++;
    String status = "active:" + String(counter);
    client.publish(topic_pub, status.c_str());
    devices[0].online = true;
    Serial.print("📤 發布: ");
    Serial.println(status);
  }
}