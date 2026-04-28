#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include "DHT.h"
#include <FluxGarage_RoboEyes.h>

// --- 硬體與 API 設定 ---
const char* ssid = "iSpan-R309";
const char* password = "66316588";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a"; 
String city = "Taipei,TW";

#define DHTPIN 2     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

#define TOUCH_PIN 15          
#define BUZZER_PIN 16
const int VOLUME = 1; 
const int longPressTime = 800; 

// --- 全域變數 ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

// 模式切換
enum DisplayMode { EYES, WEATHER, POMODORO };
DisplayMode currentMode = EYES;

// 天氣資料
float curTemp = 0.0, forTemp1 = 0.0, forTemp2 = 0.0;
int curHum = 0;
String forDate1 = "--", forDate2 = "--", forWeather1 = "--", forWeather2 = "--";

// 番茄鐘變數
int pMinutes = 25, pSeconds = 0;
bool isWorkMode = true;
bool pomodoroActive = false; // 是否正在計時

// 計時器
unsigned long lastWeatherUpdate = 0;
unsigned long sensorTimer = 0;
unsigned long pomodoroTimer = 0;
unsigned long touchStartTime = 0;
bool isTouching = false;
bool manualMoodActive = false;

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  analogWrite(BUZZER_PIN, 0);

  Wire.setSDA(4); Wire.setSCL(5);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  dht.begin();
  timeClient.begin();
  updateWeatherData();
  
  roboEyes.begin(128, 64, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
}

void loop() {
  timeClient.update();
  unsigned long currentMillis = millis();

  // 1. 背景更新 (天氣 & 番茄鐘計時)
  if (currentMillis - lastWeatherUpdate >= 1800000) {
    lastWeatherUpdate = currentMillis;
    updateWeatherData();
  }

  if (pomodoroActive && (currentMillis - pomodoroTimer >= 1000)) {
    pomodoroTimer = currentMillis;
    runPomodoroLogic();
  }

  // 2. 觸摸邏輯
  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) { touchStartTime = currentMillis; isTouching = true; }
    
    // 長按判斷
    if (currentMillis - touchStartTime > longPressTime) {
      if (currentMode == EYES) { roboEyes.setMood(HAPPY); manualMoodActive = true; }
    }
  } else {
    if (isTouching) {
      unsigned long duration = currentMillis - touchStartTime;
      if (duration < longPressTime) {
        // 短按：切換三種模式
        if (currentMode == EYES) currentMode = WEATHER;
        else if (currentMode == WEATHER) currentMode = POMODORO;
        else currentMode = EYES;
      } else {
        // 長按釋放：如果是番茄鐘，切換開關
        if (currentMode == POMODORO) pomodoroActive = !pomodoroActive;
      }
      manualMoodActive = false;
      isTouching = false;
    }
  }

  // 3. 顯示渲染
  display.clearDisplay();
  if (currentMode == EYES) {
    handleEyeLogic(currentMillis);
    roboEyes.update();
  } else if (currentMode == WEATHER) {
    drawWeatherPage();
  } else if (currentMode == POMODORO) {
    drawPomodoroPage();
  }
}

// --- 功能子函式 ---

void runPomodoroLogic() {
  if (pSeconds == 0) {
    if (pMinutes == 0) {
      isWorkMode = !isWorkMode;
      pMinutes = isWorkMode ? 25 : 5;
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
      if (t < 20) { roboEyes.setMood(TIRED); roboEyes.setHFlicker(ON, 2); }
      else if (t > 25) { roboEyes.setMood(TIRED); roboEyes.setSweat(ON); roboEyes.setPosition(S); }
      else { roboEyes.setMood(DEFAULT); roboEyes.setHFlicker(OFF); roboEyes.setSweat(OFF); roboEyes.setPosition(DEFAULT); }
    }
  }
}

void drawWeatherPage() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti = localtime(&rawtime);
  char dateBuf[12];
  sprintf(dateBuf, "%04d-%02d-%02d", ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
  
  display.setCursor(0, 0); display.print("Date: "); display.println(dateBuf);
  display.setCursor(0, 10); display.print("Time: "); display.println(timeClient.getFormattedTime());
  display.setCursor(0, 20); display.print("T: "); display.print(curTemp, 1);
  display.print("C  H: "); display.print(curHum); display.println("%");
  display.drawFastHLine(0, 30, 128, WHITE);
  display.setCursor(0, 34); display.println("Forecast:");
  display.setCursor(0, 44); display.print(forDate1); display.print(": "); display.print(forTemp1, 0); display.print("C "); display.println(forWeather1);
  display.setCursor(0, 54); display.print(forDate2); display.print(": "); display.print(forTemp2, 0); display.print("C "); display.println(forWeather2);
  display.display();
}

void drawPomodoroPage() {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(15, 0);
  display.println(isWorkMode ? ">>FOCUS<<" : "==REST==");
  display.setTextSize(3);
  display.setCursor(20, 30);
  if (pMinutes < 10) display.print("0"); display.print(pMinutes);
  display.print(":");
  if (pSeconds < 10) display.print("0"); display.print(pSeconds);
  if (!pomodoroActive) { display.setTextSize(1); display.setCursor(40, 56); display.print("-PAUSED-"); }
  display.display();
}

void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http; WiFiClient client;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&units=metric&cnt=25&appid=" + apiKey;
  http.begin(client, url);
  if (http.GET() == 200) {
    JsonDocument doc; deserializeJson(doc, http.getString());
    curTemp = doc["list"][0]["main"]["temp"];
    curHum = doc["list"][0]["main"]["humidity"];
    forDate1 = doc["list"][8]["dt_txt"].as<String>().substring(5, 10);
    forTemp1 = doc["list"][8]["main"]["temp"];
    forWeather1 = doc["list"][8]["weather"][0]["main"].as<String>();
    forDate2 = doc["list"][16]["dt_txt"].as<String>().substring(5, 10);
    forTemp2 = doc["list"][16]["main"]["temp"];
    forWeather2 = doc["list"][16]["weather"][0]["main"].as<String>();
  }
  http.end();
}

void playSoftTone(int freq, int duration) {
  analogWriteFreq(freq); analogWrite(BUZZER_PIN, VOLUME);
  delay(duration); analogWrite(BUZZER_PIN, 0);
}
void startSound() { playSoftTone(523, 100); playSoftTone(659, 100); playSoftTone(784, 200); }
void restSound() { playSoftTone(784, 100); playSoftTone(659, 100); playSoftTone(523, 200); }

