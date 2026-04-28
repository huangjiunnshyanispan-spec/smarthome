#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include "DHT.h"
#include <FluxGarage_RoboEyes.h>
#include <esp_wifi.h>

// ─── 網路設定 ────────────────────────────────────────────────
const char* ssid        = "iSpan-R309";
const char* password    = "66316588";
const char* mqtt_server = "192.168.69.138";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a";
String city   = "Taipei,TW";

const char* topic_pub = "home/smartDesk";
const char* topic_sub = "home/smartDesk/control";

// ─── 腳位定義 ────────────────────────────────────────────────
#define DHTPIN       10       // ★ 改成 GPIO 10（需外接 10kΩ 上拉）
#define DHTTYPE      DHT11
#define TOUCH_PIN    3
#define BUZZER_PIN   4
#define I2C_SDA      6        // ★ 改成 GPIO 6（原 GPIO 8 讓給 LED）
#define I2C_SCL      7        // ★ 改成 GPIO 7（原 GPIO 9 空出）
#define LED_PIN      8        // ★ ESP32-C3 內建 LED

// ★ ESP32-C3 蜂鳴器用 LEDC 替代 analogWrite
#define BUZZER_CH    0
#define BUZZER_RES   8
const int VOLUME        = 64;
const int longPressTime = 800;

// ─── OLED ────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);
DHT dht(DHTPIN, DHTTYPE, 20);

WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

// ─── 顯示模式 ────────────────────────────────────────────────
enum DisplayMode { EYES, WEATHER, POMODORO, SMARTDESK };
DisplayMode currentMode = EYES;

// ════════════════════════════════════════════════════════════
//  ★ 跨 Task 共享變數
// ════════════════════════════════════════════════════════════
volatile bool  wifiConnected = false;
volatile bool  mqttConnected = false;

volatile float curTemp    = 0.0;
volatile int   curOutHum  = 0;
volatile float forTemp1   = 0.0, forTemp2 = 0.0, forTemp3 = 0.0;
volatile char  forDate1[6]     = "--", forDate2[6]     = "--", forDate3[6]     = "--";
volatile char  forWeather1[16] = "--", forWeather2[16] = "--", forWeather3[16] = "--";

volatile unsigned long ntpEpoch   = 0;
volatile int           ledCmd     = -1;
volatile bool          reqReconnect = false;

// ★ 室內溫濕度（統一讀取，避免 DHT 過頻讀取）
volatile float indoorTemp     = NAN;
volatile int   indoorHumidity = 0;

// ─── 裝置狀態 ────────────────────────────────────────────────
const int DEVICE_COUNT = 5;
struct Device {
  const char*   label;
  const char*   topic;
  volatile bool online;
};
Device devices[DEVICE_COUNT] = {
  { "Desk   ", "home/smartDesk",        false },
  { "Feeder ", "home/petFeeder",        false },
  { "Voice  ", "home/voiceRecognition", false },
  { "Camera ", "home/surveillance",     false },
  { "Garage ", "home/smartGarage",      false }
};

// ─── 網路 Task 內部變數 ──────────────────────────────────────
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

unsigned long lastWeatherUpdate = 0;
unsigned long lastMsgTimer      = 0;
int           counter           = 0;

const unsigned long WEATHER_INTERVAL = 1800000UL;
const unsigned long PUBLISH_INTERVAL = 5000;

// ─── 顯示 Task 內部變數 ──────────────────────────────────────
unsigned long lastDisplay    = 0;
unsigned long sensorTimer    = 0;
unsigned long touchStartTime = 0;
unsigned long pomodoroTimer  = 0;
bool isTouching       = false;
bool manualMoodActive = false;

int  pMinutes       = 25, pSeconds = 0;
bool isWorkMode     = true;
bool pomodoroActive = false;

// ─── DHT 統一讀取變數 ────────────────────────────────────────
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2500;

// ════════════════════════════════════════════════════════════
//  ★ WiFi 穩定模式連線（共用函式）
// ════════════════════════════════════════════════════════════
bool stableWiFiConnect(unsigned long timeoutMs) {
  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(40);  // 限制功耗，防止重啟
  esp_wifi_set_ps(WIFI_PS_NONE);  // 保持連線不進入休眠

  WiFi.begin(ssid, password);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(500);
    Serial.printf("正在連線... 狀態: %d, 強度: %d dBm\n", WiFi.status(), WiFi.RSSI());
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi 連線成功！");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("已設定: TX Power=10dBm, 省電模式=關閉");
    return true;
  } else {
    Serial.printf("❌ WiFi 連線失敗，狀態碼: %d\n", WiFi.status());
    return false;
  }
}

// ════════════════════════════════════════════════════════════
//  ★ MQTT 連線（共用函式）
// ════════════════════════════════════════════════════════════
bool connectMQTT() {
  String clientId = "ESP32C3-SmartDesk-" + String(random(0xffff), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("✅ MQTT 連線成功");
    mqttClient.subscribe(topic_sub);
    for (int i = 0; i < DEVICE_COUNT; i++) {
      mqttClient.subscribe(devices[i].topic);
      Serial.print("   訂閱: ");
      Serial.println(devices[i].topic);
    }
    return true;
  }
  Serial.printf("❌ MQTT 失敗，狀態: %d\n", mqttClient.state());
  return false;
}

// ════════════════════════════════════════════════════════════
//  ★ DHT 統一讀取（最少間隔 2.5 秒，避免 NaN）
// ════════════════════════════════════════════════════════════
void updateDHTReading(unsigned long now) {
  if (now - lastDHTRead < DHT_INTERVAL) return;
  lastDHTRead = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    indoorTemp     = t;
    indoorHumidity = (int)h;
  }
}

// ════════════════════════════════════════════════════════════
//  蜂鳴器
// ════════════════════════════════════════════════════════════
void playSoftTone(int freq, int duration) {
  ledcAttach(BUZZER_PIN, freq, BUZZER_RES);
  ledcWrite(BUZZER_PIN, VOLUME);
  delay(duration);
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
  pinMode(BUZZER_PIN, INPUT);
}
void startSound()     { playSoftTone(523,100); playSoftTone(659,100); playSoftTone(784,200); }
void restSound()      { playSoftTone(784,100); playSoftTone(659,100); playSoftTone(523,200); }
void pauseSound()     { playSoftTone(440,80);  playSoftTone(330,160); }
void reconnectSound() { playSoftTone(440,80);  playSoftTone(440,80); }

// ════════════════════════════════════════════════════════════
//  天氣圖示
// ═════════════════════���══════════════════════════════════════
void drawWeatherIcon(int x, int y, String weather) {
  weather.toLowerCase();
  if (weather.indexOf("clear") >= 0 || weather.indexOf("sun") >= 0) {
    display.fillCircle(x+9, y+9, 4, WHITE);
    display.drawFastVLine(x+9, y,    3, WHITE);
    display.drawFastVLine(x+9, y+15, 3, WHITE);
    display.drawFastHLine(x,    y+9, 3, WHITE);
    display.drawFastHLine(x+15, y+9, 3, WHITE);
    display.drawLine(x+3, y+3,  x+4,  y+4,  WHITE);
    display.drawLine(x+14,y+3,  x+13, y+4,  WHITE);
    display.drawLine(x+3, y+14, x+4,  y+13, WHITE);
    display.drawLine(x+14,y+14, x+13, y+13, WHITE);
  } else if (weather.indexOf("cloud") >= 0) {
    display.fillCircle(x+5,  y+10, 5, WHITE);
    display.fillCircle(x+9,  y+7,  6, WHITE);
    display.fillCircle(x+14, y+10, 4, WHITE);
    display.fillRect(x, y+10, 18, 5, WHITE);
    display.drawFastHLine(x, y+14, 18, WHITE);
  } else if (weather.indexOf("rain") >= 0 || weather.indexOf("drizzle") >= 0) {
    display.fillCircle(x+5,  y+7, 4, WHITE);
    display.fillCircle(x+9,  y+5, 5, WHITE);
    display.fillCircle(x+13, y+7, 3, WHITE);
    display.fillRect(x+1, y+7, 14, 4, WHITE);
    display.drawLine(x+2,  y+12, x+1,  y+16, WHITE);
    display.drawLine(x+6,  y+12, x+5,  y+16, WHITE);
    display.drawLine(x+10, y+12, x+9,  y+16, WHITE);
    display.drawLine(x+14, y+12, x+13, y+16, WHITE);
  } else if (weather.indexOf("snow") >= 0) {
    display.drawFastHLine(x+1, y+9, 16, WHITE);
    display.drawFastVLine(x+9, y+1, 16, WHITE);
    display.drawLine(x+3, y+3,  x+15, y+15, WHITE);
    display.drawLine(x+15,y+3,  x+3,  y+15, WHITE);
    display.fillCircle(x+1,  y+9,  1, WHITE);
    display.fillCircle(x+16, y+9,  1, WHITE);
    display.fillCircle(x+9,  y+1,  1, WHITE);
    display.fillCircle(x+9,  y+16, 1, WHITE);
  } else if (weather.indexOf("thunder") >= 0 || weather.indexOf("storm") >= 0) {
    display.fillCircle(x+4,  y+6, 4, WHITE);
    display.fillCircle(x+9,  y+4, 5, WHITE);
    display.fillCircle(x+13, y+6, 3, WHITE);
    display.fillRect(x+1, y+6, 13, 3, WHITE);
    display.drawLine(x+9, y+10, x+6, y+13, WHITE);
    display.drawLine(x+6, y+13, x+10,y+13, WHITE);
    display.drawLine(x+10,y+13, x+7, y+17, WHITE);
  } else if (weather.indexOf("mist") >= 0 || weather.indexOf("fog") >= 0 || weather.indexOf("haze") >= 0) {
    display.drawFastHLine(x+1, y+3,  16, WHITE);
    display.drawFastHLine(x+3, y+7,  12, WHITE);
    display.drawFastHLine(x+1, y+11, 16, WHITE);
    display.drawFastHLine(x+3, y+15, 12, WHITE);
  } else {
    display.setCursor(x+5, y+5);
    display.setTextSize(1);
    display.print("?");
  }
}

// ════════════════════════════════════════════════════════════
//  繪製各頁面
// ════════════════════════════════════════════════════════════
void drawWeatherPage() {
  display.setTextColor(WHITE);
  time_t rawtime = (time_t)ntpEpoch;
  struct tm* ti  = localtime(&rawtime);
  char topBuf[22];
  sprintf(topBuf, "%04d-%02d-%02d %02d:%02d:%02d",
          ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday,
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(topBuf);

  display.setCursor(0, 11);
  display.print("I:");
  if (!isnan(indoorTemp)) {
    display.print((int)round(indoorTemp));
    display.print("C ");
    display.print(indoorHumidity);
    display.print("%  ");
  } else {
    display.print("--C --%  ");
  }
  display.print("O:");
  display.print((int)round(curTemp));
  display.print("C ");
  display.print(curOutHum);
  display.print("%");

  const int colCenter[3] = {21, 63, 105};
  String dates[3]   = { String(const_cast<const char*>(forDate1)),    String(const_cast<const char*>(forDate2)),    String(const_cast<const char*>(forDate3)) };
  int    temps[3]   = { (int)round(forTemp1),(int)round(forTemp2),(int)round(forTemp3) };
  String weathrs[3] = { String(const_cast<const char*>(forWeather1)), String(const_cast<const char*>(forWeather2)), String(const_cast<const char*>(forWeather3)) };

  for (int i = 0; i < 3; i++) {
    int cx = colCenter[i];
    display.setTextSize(1);
    display.setCursor(cx - 14, 22);
    display.print(dates[i]);
    drawWeatherIcon(cx - 9, 32, weathrs[i]);
    display.setCursor(cx - 9, 55);
    if (temps[i] >= 0 && temps[i] < 10) display.setCursor(cx - 6, 55);
    display.print(temps[i]);
    display.print("C");
  }
  display.display();
}

void drawPomodoroPage() {
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(15, 0);
  display.println(isWorkMode ? ">>FOCUS<<" : "==REST==");
  display.setTextSize(3);
  display.setCursor(20, 30);
  if (pMinutes < 10) display.print("0");
  display.print(pMinutes);
  display.print(":");
  if (pSeconds < 10) display.print("0");
  display.print(pSeconds);
  if (!pomodoroActive) {
    display.setTextSize(1);
    display.setCursor(40, 56);
    display.print("-PAUSED-");
  }
  display.display();
}

void drawSmartDeskPage() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi:");
  display.print(wifiConnected ? "OK" : "NG");
  display.print(" MQTT:");
  display.print(mqttConnected ? "OK" : "NG");

  if (!wifiConnected || !mqttConnected) {
    display.setCursor(0, 10);
    display.print("Long press=Reconnect");
  }

  for (int i = 0; i < DEVICE_COUNT; i++) {
    int y = (!wifiConnected || !mqttConnected) ? 22 + i * 8 : 12 + i * 10;
    display.setCursor(0, y);
    display.print(devices[i].label);
    display.print(devices[i].online ? "ON " : "OFF");
  }
  display.display();
}

// ════════════════════════════════════════════════════════════
//  番茄鐘
// ════════════════════════════════════════════════════════════
void runPomodoroLogic() {
  if (pSeconds == 0) {
    if (pMinutes == 0) {
      isWorkMode = !isWorkMode;
      pMinutes   = isWorkMode ? 25 : 5;
      if (isWorkMode) startSound(); else restSound();
    } else { pMinutes--; pSeconds = 59; }
  } else { pSeconds--; }
}

// ════════════════════════════════════════════════════════════
//  眼睛情緒（修正偏移問題）
// ════════════════════════════════════════════════════════════
void handleEyeLogic(unsigned long now) {
  if (now - sensorTimer >= 5000) {
    sensorTimer = now;

    float t = indoorTemp;
    if (isnan(t)) return;

    if (!manualMoodActive) {
      if (t < 20) {
        roboEyes.setMood(TIRED);
        roboEyes.setHFlicker(ON, 2);
        roboEyes.setSweat(OFF);
        roboEyes.setPosition(DEFAULT);
      } else if (t > 25) {
        roboEyes.setMood(TIRED);
        roboEyes.setSweat(ON);
        roboEyes.setHFlicker(OFF);
        roboEyes.setPosition(DEFAULT);
      } else {
        roboEyes.setMood(DEFAULT);
        roboEyes.setHFlicker(OFF);
        roboEyes.setSweat(OFF);
        roboEyes.setPosition(DEFAULT);
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
//  MQTT Callback
// ════════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message  = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  Serial.printf("📩 [%s] -> %s\n", topicStr.c_str(), message.c_str());

  if (topicStr == String(topic_sub)) {
    if (message == "on")  ledCmd = 1;
    if (message == "off") ledCmd = 0;
    return;
  }
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (topicStr == String(devices[i].topic)) {
      devices[i].online = (message != "offline");
      return;
    }
  }
}

// ════════════════════════════════════════════════════════════
//  天氣更新
// ════════════════════════════════════════════════════════════
void updateWeatherData() {
  if (!wifiConnected) return;
  HTTPClient http;
  WiFiClient httpClient;
  http.setTimeout(8000);
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city +
               "&units=metric&cnt=25&appid=" + apiKey;
  http.begin(httpClient, url);
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    String body = http.getString();
    if (!deserializeJson(doc, body)) {
      curTemp   = doc["list"][0]["main"]["temp"];
      curOutHum = doc["list"][0]["main"]["humidity"];

      String d1 = doc["list"][8]["dt_txt"].as<String>().substring(5,10);
      String w1 = doc["list"][8]["weather"][0]["main"].as<String>();
      strncpy((char*)forDate1,    d1.c_str(), 5);  forDate1[5]    = 0;
      strncpy((char*)forWeather1, w1.c_str(), 15); forWeather1[15]= 0;
      forTemp1 = doc["list"][8]["main"]["temp"];

      String d2 = doc["list"][16]["dt_txt"].as<String>().substring(5,10);
      String w2 = doc["list"][16]["weather"][0]["main"].as<String>();
      strncpy((char*)forDate2,    d2.c_str(), 5);  forDate2[5]    = 0;
      strncpy((char*)forWeather2, w2.c_str(), 15); forWeather2[15]= 0;
      forTemp2 = doc["list"][16]["main"]["temp"];

      String d3 = doc["list"][24]["dt_txt"].as<String>().substring(5,10);
      String w3 = doc["list"][24]["weather"][0]["main"].as<String>();
      strncpy((char*)forDate3,    d3.c_str(), 5);  forDate3[5]    = 0;
      strncpy((char*)forWeather3, w3.c_str(), 15); forWeather3[15]= 0;
      forTemp3 = doc["list"][24]["main"]["temp"];
    }
  }
  http.end();
}

// ════════════════════════════════════════════════════════════
//  ★ 手動重連（使用穩定模式）
// ════════════════════════════════════════════════════════════
void doManualReconnect() {
  mqttClient.disconnect();
  wifiConnected = false;
  mqttConnected = false;
  for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;

  if (!stableWiFiConnect(10000)) return;
  wifiConnected = true;

  if (connectMQTT()) {
    mqttConnected = true;
  }

  timeClient.update();
  ntpEpoch = timeClient.getEpochTime();
  updateWeatherData();
  lastWeatherUpdate = millis();
}

// ════════════════════════════════════════════════════════════
//  ★ 網路 Task（FreeRTOS，背景執行）
// ════════════════════════════════════════════════════════════
void networkTask(void* pvParameters) {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  if (stableWiFiConnect(20000)) {
    wifiConnected = true;

    if (connectMQTT()) {
      mqttConnected = true;
    }

    timeClient.begin();
    timeClient.update();
    ntpEpoch = timeClient.getEpochTime();
    updateWeatherData();
    lastWeatherUpdate = millis();
  }

  static unsigned long lastNtp     = 0;
  static unsigned long lastSecTick = 0;

  for (;;) {
    unsigned long now = millis();

    if (reqReconnect) {
      reqReconnect = false;
      doManualReconnect();
    }

    bool wifiNow = (WiFi.status() == WL_CONNECTED);
    bool mqttNow = mqttClient.connected();

    if (!wifiNow && wifiConnected) {
      wifiConnected = false;
      mqttConnected = false;
      for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
    } else if (wifiNow && !wifiConnected) {
      wifiConnected = true;
    }

    if (!mqttNow && mqttConnected) {
      mqttConnected = false;
      for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
    }

    if (wifiConnected && !mqttConnected) {
      if (connectMQTT()) {
        mqttConnected = true;
      }
    }

    if (mqttNow) mqttClient.loop();

    if (wifiConnected && now - lastNtp >= 60000) {
      lastNtp = now;
      timeClient.update();
      ntpEpoch = timeClient.getEpochTime();
    } else if (now - lastSecTick >= 1000) {
      lastSecTick = now;
      if (ntpEpoch > 0) ntpEpoch++;
    }

    if (wifiConnected && now - lastWeatherUpdate >= WEATHER_INTERVAL) {
      lastWeatherUpdate = now;
      updateWeatherData();
    }

    if (mqttConnected && now - lastMsgTimer >= PUBLISH_INTERVAL) {
      lastMsgTimer = now;
      counter++;
      String status = "active:" + String(counter);
      mqttClient.publish(topic_pub, status.c_str());
      devices[0].online = true;
    }

    // ★ LED 指令（使用 LED_PIN 而非 LED_BUILTIN）
    if (ledCmd == 1) { digitalWrite(LED_PIN, LOW); Serial.println("💡 LED ON");  ledCmd = -1; }
    if (ledCmd == 0) { digitalWrite(LED_PIN, HIGH);  Serial.println("💡 LED OFF"); ledCmd = -1; }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ════════════════════════════════════════════════════════════
//  setup / loop
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(LED_PIN, OUTPUT);       // ★ 使用明確的 GPIO 8
  digitalWrite(LED_PIN, HIGH);     // ★ 預設關閉

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(DHTPIN, INPUT_PULLUP);  // ★ 開啟內部上拉（搭配外接 10kΩ）

  Wire.begin(I2C_SDA, I2C_SCL);  // ★ GPIO 6, GPIO 7

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(8, 10);
  display.println("SmartDesk");
  display.display();
  delay(1000);

  dht.begin();

  roboEyes.begin(128, 64, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);
  roboEyes.setHFlicker(OFF);
  roboEyes.setSweat(OFF);

  // ★ 新增：讓 roboEyes 先跑幾幀穩定��置，防止開機偏移
  for (int i = 0; i < 5; i++) {
    display.clearDisplay();
    roboEyes.update();
    delay(50);
  }
  xTaskCreate(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    NULL
  );
}

void loop() {
  unsigned long now = millis();

  updateDHTReading(now);

  if (pomodoroActive && now - pomodoroTimer >= 1000) {
    pomodoroTimer = now;
    runPomodoroLogic();
  }

  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) { touchStartTime = now; isTouching = true; }

    if (now - touchStartTime > longPressTime) {
      if (currentMode == EYES && wifiConnected && mqttConnected) {
        roboEyes.setMood(HAPPY);
        manualMoodActive = true;
      }
    }
  } else {
    if (isTouching) {
      unsigned long duration = now - touchStartTime;
      bool offline = !wifiConnected || !mqttConnected;

      if (duration < longPressTime) {
        if      (currentMode == EYES)      currentMode = WEATHER;
        else if (currentMode == WEATHER)   currentMode = POMODORO;
        else if (currentMode == POMODORO)  currentMode = SMARTDESK;
        else                               currentMode = EYES;
        manualMoodActive = false;

        if (currentMode == EYES) {
          roboEyes.setMood(DEFAULT);
          roboEyes.setHFlicker(OFF);
          roboEyes.setSweat(OFF);
          roboEyes.setPosition(DEFAULT);
        }

      } else {
        if (offline && currentMode == SMARTDESK) {
          reconnectSound();
          reqReconnect = true;
        } else if (currentMode == POMODORO) {
          pomodoroActive = !pomodoroActive;
          if (pomodoroActive) startSound();
          else                pauseSound();
        } else if (currentMode == EYES) {
          manualMoodActive = false;
        }
      }
      isTouching = false;
    }
  }

  if (currentMode == EYES) {
    handleEyeLogic(now);
    display.clearDisplay();
    roboEyes.update();
  }

  if (currentMode != EYES && now - lastDisplay >= 500) {
    lastDisplay = now;
    display.clearDisplay();
    if      (currentMode == WEATHER)   drawWeatherPage();
    else if (currentMode == POMODORO)  drawPomodoroPage();
    else if (currentMode == SMARTDESK) drawSmartDeskPage();
  }
}