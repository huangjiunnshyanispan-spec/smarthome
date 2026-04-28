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

const char* ssid        = "iSpan-R309";
const char* password    = "66316588";
const char* mqtt_server = "192.168.39.107";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a";
String city   = "Taipei,TW";

const char* topic_pub = "home/smartDesk";
const char* topic_sub = "home/smartDesk/control";

#define DHTPIN      2
#define DHTTYPE     DHT11
#define TOUCH_PIN   15
#define BUZZER_PIN  16
const int VOLUME        = 1;
const int longPressTime = 800;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);
DHT dht(DHTPIN, DHTTYPE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

enum DisplayMode { EYES, WEATHER, POMODORO, SMARTDESK };
DisplayMode currentMode = EYES;

float  curTemp   = 0.0;
int    curOutHum = 0;
float  forTemp1  = 0.0, forTemp2  = 0.0, forTemp3  = 0.0;
String forDate1  = "--", forDate2  = "--", forDate3  = "--";
String forWeather1 = "--", forWeather2 = "--", forWeather3 = "--";
unsigned long lastWeatherUpdate = 0;

int  pMinutes       = 25, pSeconds = 0;
bool isWorkMode     = true;
bool pomodoroActive = false;
unsigned long pomodoroTimer = 0;

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
bool wifiConnected = false;
bool mqttConnected = false;

WiFiClient   espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMsg        = 0;
unsigned long lastDisplay    = 0;
unsigned long sensorTimer    = 0;
unsigned long touchStartTime = 0;
bool isTouching       = false;
bool manualMoodActive = false;
int  counter          = 0;

void playSoftTone(int freq, int duration) {
  pinMode(BUZZER_PIN, OUTPUT);
  analogWriteFreq(freq);
  analogWrite(BUZZER_PIN, VOLUME);
  delay(duration);
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUZZER_PIN, INPUT);
}
void startSound() { playSoftTone(523,100); playSoftTone(659,100); playSoftTone(784,200); }
void restSound()  { playSoftTone(784,100); playSoftTone(659,100); playSoftTone(523,200); }
void pauseSound() { playSoftTone(440,80);  playSoftTone(330,160); }

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

void drawWeatherPage() {
  display.setTextColor(WHITE);
  time_t rawtime = timeClient.getEpochTime();
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
  String dates[3]   = {forDate1,    forDate2,    forDate3};
  int    temps[3]   = {(int)round(forTemp1), (int)round(forTemp2), (int)round(forTemp3)};
  String weathrs[3] = {forWeather1, forWeather2, forWeather3};

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
  for (int i = 0; i < DEVICE_COUNT; i++) {
    int y = 12 + i * 10;
    display.setCursor(0, y);
    display.print(devices[i].label);
    display.print(devices[i].online ? "ON " : "OFF");
  }
  display.display();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String message  = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  if (topicStr == String(topic_sub)) {
    if (message == "on")  digitalWrite(LED_BUILTIN, HIGH);
    if (message == "off") digitalWrite(LED_BUILTIN, LOW);
    return;
  }

  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (topicStr == String(devices[i].topic)) {
      devices[i].online = (message != "offline");
      return;
    }
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  wifiConnected = true;
}

void mqttReconnect() {
  mqttConnected = false;
  for (int i = 0; i < DEVICE_COUNT; i++) devices[i].online = false;
  while (!mqttClient.connected()) {
    String clientId = "PicoW-SmartDesk-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      mqttConnected = true;
      mqttClient.subscribe(topic_sub);
      for (int i = 0; i < DEVICE_COUNT; i++) {
        mqttClient.subscribe(devices[i].topic);
      }
    } else {
      delay(2000);
    }
  }
}

void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  WiFiClient httpClient;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city +
               "&units=metric&cnt=25&appid=" + apiKey;
  http.begin(httpClient, url);
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    curTemp      = doc["list"][0]["main"]["temp"];
    curOutHum    = doc["list"][0]["main"]["humidity"];
    forDate1     = doc["list"][8]["dt_txt"].as<String>().substring(5,10);
    forTemp1     = doc["list"][8]["main"]["temp"];
    forWeather1  = doc["list"][8]["weather"][0]["main"].as<String>();
    forDate2     = doc["list"][16]["dt_txt"].as<String>().substring(5,10);
    forTemp2     = doc["list"][16]["main"]["temp"];
    forWeather2  = doc["list"][16]["weather"][0]["main"].as<String>();
    forDate3     = doc["list"][24]["dt_txt"].as<String>().substring(5,10);
    forTemp3     = doc["list"][24]["main"]["temp"];
    forWeather3  = doc["list"][24]["weather"][0]["main"].as<String>();
  }
  http.end();
}

void runPomodoroLogic() {
  if (pSeconds == 0) {
    if (pMinutes == 0) {
      isWorkMode = !isWorkMode;
      pMinutes   = isWorkMode ? 25 : 5;
      if (isWorkMode) startSound(); else restSound();
    } else { pMinutes--; pSeconds = 59; }
  } else { pSeconds--; }
}

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

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, INPUT);

  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(8, 10);
  display.println("SmartDesk");
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println("Dashboard v2.0");
  display.setCursor(28, 52);
  display.println("Starting...");
  display.display();
  delay(2000);

  setup_wifi();
  timeClient.begin();
  updateWeatherData();
  dht.begin();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
  roboEyes.begin(128, 64, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
}

void loop() {
  unsigned long now = millis();

  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (!wifiConnected) setup_wifi();

  if (!mqttClient.connected()) {
    mqttConnected = false;
    mqttReconnect();
  }
  mqttClient.loop();

  timeClient.update();

  if (now - lastWeatherUpdate >= 1800000) {
    lastWeatherUpdate = now;
    updateWeatherData();
  }

  if (pomodoroActive && (now - pomodoroTimer >= 1000)) {
    pomodoroTimer = now;
    runPomodoroLogic();
  }

  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) { touchStartTime = now; isTouching = true; }
    if (now - touchStartTime > longPressTime) {
      if (currentMode == EYES) {
        roboEyes.setMood(HAPPY);
        manualMoodActive = true;
      }
    }
  } else {
    if (isTouching) {
      unsigned long duration = now - touchStartTime;
      if (duration < longPressTime) {
        if      (currentMode == EYES)      currentMode = WEATHER;
        else if (currentMode == WEATHER)   currentMode = POMODORO;
        else if (currentMode == POMODORO)  currentMode = SMARTDESK;
        else                               currentMode = EYES;
      } else {
        if (currentMode == POMODORO) {
          pomodoroActive = !pomodoroActive;
          if (pomodoroActive) startSound();
          else                pauseSound();
        }
      }
      manualMoodActive = false;
      isTouching = false;
    }
  }

  // 眼睛頁：不受 500ms 限制，每次 loop 都更新
  if (currentMode == EYES) {
    handleEyeLogic(now);
    display.clearDisplay();
    roboEyes.update();
  }

  // 其他頁：每 500ms 更新一次
  if (currentMode != EYES && now - lastDisplay > 500) {
    lastDisplay = now;
    display.clearDisplay();
    if      (currentMode == WEATHER)   { drawWeatherPage(); }
    else if (currentMode == POMODORO)  { drawPomodoroPage(); }
    else if (currentMode == SMARTDESK) { drawSmartDeskPage(); }
  }

  // 每 5 秒發布自身狀態
  if (now - lastMsg > 5000) {
    lastMsg = now;
    counter++;
    String status = "active:" + String(counter);
    mqttClient.publish(topic_pub, status.c_str());
    devices[0].online = true;
  }
}