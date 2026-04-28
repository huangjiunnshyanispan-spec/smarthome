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

// ─── 網路設定 ────────────────────────────────────────────────
const char* ssid        = "iSpan-R309";
const char* password    = "66316588";
const char* mqtt_server = "192.168.39.107";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a";
String city   = "Taipei,TW";

const char* topic_pub = "home/smartDesk";
const char* topic_sub = "home/smartDesk/control";

// ─── 腳位定義 ────────────────────────────────────────────────
#define DHTPIN      2
#define DHTTYPE     DHT11
#define TOUCH_PIN   15
#define BUZZER_PIN  16
const int VOLUME        = 1;
const int longPressTime = 800;

// ─── OLED ────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);
DHT dht(DHTPIN, DHTTYPE);

WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

// ─── 顯示模式 ────────────────────────────────────────────────
enum DisplayMode { EYES, WEATHER, POMODORO, SMARTDESK };
DisplayMode currentMode = EYES;

// ════════════════════════════════════════════════════════════
//  跨核心共享變數（Core1 寫，Core0 讀）
// ════════════════════════════════════════════════════════════
volatile bool  wifiConnected = false;
volatile bool  mqttConnected = false;

volatile float curTemp    = 0.0;
volatile int   curOutHum  = 0;
volatile float forTemp1   = 0.0, forTemp2  = 0.0, forTemp3  = 0.0;
volatile char  forDate1[6]    = "--", forDate2[6]    = "--", forDate3[6]    = "--";
volatile char  forWeather1[16]= "--", forWeather2[16]= "--", forWeather3[16]= "--";

volatile unsigned long ntpEpoch = 0;

volatile int  ledCmd    = -1;   // -1=無動作, 0=關, 1=開

// ★ Core0 → Core1 的重連請求旗標
volatile bool reqReconnect = false;

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

// ─── Core1 內部變數 ──────────────────────────────────────────
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

bool          wifiAttempting    = false;
bool          manualReconnect   = false;  // ★ 標記本次是手動觸發
unsigned long wifiRetryTimer    = 0;
unsigned long mqttRetryTimer    = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastMsgTimer      = 0;
int           counter           = 0;

const unsigned long WIFI_RETRY_INTERVAL = 5000;
const unsigned long MQTT_RETRY_INTERVAL = 3000;
const unsigned long WEATHER_INTERVAL    = 1800000UL;
const unsigned long PUBLISH_INTERVAL    = 5000;

// ─── Core0 內部變數 ──────────────────────────────────────────
unsigned long lastDisplay    = 0;
unsigned long sensorTimer    = 0;
unsigned long touchStartTime = 0;
unsigned long pomodoroTimer  = 0;
bool isTouching       = false;
bool manualMoodActive = false;

int  pMinutes       = 25, pSeconds = 0;
bool isWorkMode     = true;
bool pomodoroActive = false;

// ════════════════════════════════════════════════════════════
//  蜂鳴器（Core0）
// ════════════════════════════════════════════════════════════
void playSoftTone(int freq, int duration) {
  pinMode(BUZZER_PIN, OUTPUT);
  analogWriteFreq(freq);
  analogWrite(BUZZER_PIN, VOLUME);
  delay(duration);
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUZZER_PIN, INPUT);
}
void startSound()    { playSoftTone(523,100); playSoftTone(659,100); playSoftTone(784,200); }
void restSound()     { playSoftTone(784,100); playSoftTone(659,100); playSoftTone(523,200); }
void pauseSound()    { playSoftTone(440,80);  playSoftTone(330,160); }
// ★ 重連提示音：兩短音
void reconnectSound(){ playSoftTone(440,80);  playSoftTone(440,80); }
// ★ 重連成功音：升音
void reconnectOkSound(){ playSoftTone(523,80); playSoftTone(784,160); }

// ════════════════════════════════════════════════════════════
//  天氣圖示（Core0）
// ════════════════════════════════════════════════════════════
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
//  繪製各頁面（Core0）
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

  float indoorT = dht.readTemperature();
  int   indoorH = (int)dht.readHumidity();
  display.setCursor(0, 11);
  display.print("I:");
  if (!isnan(indoorT)) {
    display.print((int)round(indoorT));
    display.print("C ");
    display.print(indoorH);
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

// ★ SmartDesk 頁：斷線時顯示提示說明如何重連
void drawSmartDeskPage() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi:");
  display.print(wifiConnected ? "OK" : "NG");
  display.print(" MQTT:");
  display.print(mqttConnected ? "OK" : "NG");

  // ★ 斷線時第二行顯示重連提示
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
//  番茄鐘（Core0）
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
//  眼睛情緒（Core0）
// ════════════════════════════════════════════════════════════
void handleEyeLogic(unsigned long now) {
  if (now - sensorTimer >= 5000) {
    sensorTimer = now;
    float t = dht.readTemperature();
    if (isnan(t)) return;
    if (!manualMoodActive) {
      if (t < 20) {
        roboEyes.setMood(TIRED); roboEyes.setHFlicker(ON, 2);
      } else if (t > 25) {
        roboEyes.setMood(TIRED); roboEyes.setSweat(ON); roboEyes.setPosition(S);
      } else {
        roboEyes.setMood(DEFAULT); roboEyes.setHFlicker(OFF);
        roboEyes.setSweat(OFF);    roboEyes.setPosition(DEFAULT);
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
//  Core1：MQTT Callback
// ════════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message  = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

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
//  Core1：Wi-Fi 連線（自動模式，僅在 reqReconnect=false 時輪詢）
// ════════════════════════════════════════════════════════════
void tryWifiConnect() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected  = true;
    wifiAttempting = false;
    return;
  }
  wifiConnected = false;
  unsigned long now = millis();
  if (!wifiAttempting) {
    WiFi.begin(ssid, password);
    wifiAttempting = true;
    wifiRetryTimer = now;
  } else if (now - wifiRetryTimer >= WIFI_RETRY_INTERVAL) {
    WiFi.disconnect();
    wifiAttempting = false;
  }
}

// ════════════════════════════════════════════════════════════
//  Core1：MQTT 連線
// ════════════════════════════════════════════════════════════
void tryMqttConnect() {
  if (mqttClient.connected()) {
    mqttConnected = true;
    return;
  }
  mqttConnected = false;
  for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
  if (!wifiConnected) return;

  unsigned long now = millis();
  if (now - mqttRetryTimer < MQTT_RETRY_INTERVAL) return;
  mqttRetryTimer = now;

  String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    mqttConnected = true;
    mqttClient.subscribe(topic_sub);
    for (int i = 0; i < DEVICE_COUNT; i++)
      mqttClient.subscribe(devices[i].topic);
  }
}

// ════════════════════════════════════════════════════════════
//  ★ Core1：手動重連（Core0 長按觸發後呼叫）
//    強制斷線再重連，並補抓天氣與 NTP
// ════════════════════════════════════════════════════════════
void doManualReconnect() {
  // 重置網路狀態
  mqttClient.disconnect();
  WiFi.disconnect();
  wifiConnected  = false;
  mqttConnected  = false;
  wifiAttempting = false;
  for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;

  // 重新連 Wi-Fi（Core1 這裡等最多 10 秒）
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(100);

  if (WiFi.status() != WL_CONNECTED) return;   // 還是連不上就放棄，等下次

  wifiConnected  = true;
  wifiAttempting = false;

  // 重新連 MQTT
  String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    mqttConnected = true;
    mqttClient.subscribe(topic_sub);
    for (int i = 0; i < DEVICE_COUNT; i++)
      mqttClient.subscribe(devices[i].topic);
  }

  // 補更新 NTP 與天氣
  timeClient.update();
  ntpEpoch = timeClient.getEpochTime();
  updateWeatherData();
  lastWeatherUpdate = millis();
}

// ════════════════════════════════════════════════════════════
//  Core1：天氣更新
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
//  Core0：setup / loop
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, INPUT);

  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) for (;;);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(8, 10);
  display.println("SmartDesk");
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println("Dashboard v5.0");
  display.setCursor(28, 52);
  display.println("Starting...");
  display.display();
  delay(2000);

  dht.begin();
  roboEyes.begin(128, 64, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
}

void loop() {
  unsigned long now = millis();

  // ── 番茄鐘倒數 ───────────────────────────────────────────
  if (pomodoroActive && now - pomodoroTimer >= 1000) {
    pomodoroTimer = now;
    runPomodoroLogic();
  }

  // ── 觸控處理 ─────────────────────────────────────────────
  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) { touchStartTime = now; isTouching = true; }

    // 長按進行中：顯示長按提示（僅在 SMARTDESK 頁且斷線時）
    if (now - touchStartTime > longPressTime) {
      bool offline = !wifiConnected || !mqttConnected;
      if (currentMode == EYES && !offline) {
        // 連線正常：長按 EYES 頁設定 HAPPY 情緒
        roboEyes.setMood(HAPPY);
        manualMoodActive = true;
      }
    }
  } else {
    if (isTouching) {
      unsigned long duration = now - touchStartTime;
      bool offline = !wifiConnected || !mqttConnected;

      if (duration < longPressTime) {
        // ── 短按：切換頁面 ──
        if      (currentMode == EYES)      currentMode = WEATHER;
        else if (currentMode == WEATHER)   currentMode = POMODORO;
        else if (currentMode == POMODORO)  currentMode = SMARTDESK;
        else                               currentMode = EYES;
        manualMoodActive = false;

      } else {
        // ── 長按 ──
        if (offline && currentMode == SMARTDESK) {
          // ★ 第四頁面斷線時長按 → 觸發手動重連
          reconnectSound();           // 提示音：兩短音
          reqReconnect = true;        // 通知 Core1 執行重連
        } else if (currentMode == POMODORO) {
          // 連線正常：POMODORO 頁長按 → 暫停/繼續
          pomodoroActive = !pomodoroActive;
          if (pomodoroActive) startSound();
          else                pauseSound();
        } else if (currentMode == EYES) {
          // 連線正常：EYES 頁長按已在按住期間設定，此處 reset
          manualMoodActive = false;
        }
      }
      isTouching = false;
    }
  }

  // ── 顯示更新 ─────────────────────────────────────────────
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

// ════════════════════════════════════════════════════════════
//  Core1：setup1 / loop1
// ════════════════════════════════════════════════════════════
void setup1() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  // 開機自動連線（Core1 等最多 10 秒）
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(100);

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected  = true;
    wifiAttempting = false;

    // ★ 開機連 MQTT
    String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      mqttConnected = true;
      mqttClient.subscribe(topic_sub);
      for (int i = 0; i < DEVICE_COUNT; i++)
        mqttClient.subscribe(devices[i].topic);
    }

    timeClient.begin();
    timeClient.update();
    ntpEpoch = timeClient.getEpochTime();
    updateWeatherData();
    lastWeatherUpdate = millis();
  }
}

void loop1() {
  unsigned long now = millis();

  // ★ 優先處理手動重連請求
  if (reqReconnect) {
    reqReconnect = false;
    doManualReconnect();
    // 重連結果回饋給 Core0（透過 wifiConnected / mqttConnected 即可）
    // 若成功，在 Core0 下次長按放開時可看到狀態已更新
    return;  // 本次 loop1 不再做其他事
  }

  // ★ 每次 loop 都檢查實際連線狀態，確保旗標與現實同步
  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  bool mqttNow = mqttClient.connected();

  // Wi-Fi 狀態同步
  if (!wifiNow && wifiConnected) {
    // Wi-Fi 剛斷
    wifiConnected = false;
    mqttConnected = false;
    for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
  } else if (wifiNow && !wifiConnected) {
    // Wi-Fi 剛回來（手動重連後）
    wifiConnected = true;
  }

  // MQTT 狀態同步
  if (!mqttNow && mqttConnected) {
    // MQTT 剛斷
    mqttConnected = false;
    for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
  }

  // 維持 MQTT 心跳（只有連線中才呼叫）
  if (mqttNow) mqttClient.loop();

  // NTP：每分鐘同步一次，其餘時間本地 +1
  static unsigned long lastNtp     = 0;
  static unsigned long lastSecTick = 0;
  if (wifiConnected && now - lastNtp >= 60000) {
    lastNtp = now;
    timeClient.update();
    ntpEpoch = timeClient.getEpochTime();
  } else if (now - lastSecTick >= 1000) {
    lastSecTick = now;
    if (ntpEpoch > 0) ntpEpoch++;   // 本地推進（NTP 取得後才推進）
  }

  // 天氣：每 30 分鐘更新
  if (wifiConnected && now - lastWeatherUpdate >= WEATHER_INTERVAL) {
    lastWeatherUpdate = now;
    updateWeatherData();
  }

  // 每 5 秒發布自身狀態
  if (mqttConnected && now - lastMsgTimer >= PUBLISH_INTERVAL) {
    lastMsgTimer = now;
    counter++;
    String status = "active:" + String(counter);
    mqttClient.publish(topic_pub, status.c_str());
    devices[0].online = true;
  }

  // LED 指令
  if (ledCmd == 1) { digitalWrite(LED_BUILTIN, HIGH); ledCmd = -1; }
  if (ledCmd == 0) { digitalWrite(LED_BUILTIN, LOW);  ledCmd = -1; }
}
