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

// --- 設定區 ---
const char* ssid = "iSpan-R309";
const char* password = "66316588";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a"; 
String city = "Taipei,TW";

#define DHTPIN 2     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

#define TOUCH_PIN 15          
const int longPressTime = 800; 
unsigned long touchStartTime = 0;
bool isTouching = false;
bool manualMoodActive = false;

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

// --- 天氣與時間變數 ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);
unsigned long lastWeatherUpdate = 0;
const long weatherInterval = 1800000; 
float curTemp = 0.0, forTemp1 = 0.0, forTemp2 = 0.0;
int curHum = 0;
String forDate1 = "--", forDate2 = "--", forWeather1 = "--", forWeather2 = "--";

// --- 狀態控制 ---
enum DisplayMode { EYES, WEATHER };
DisplayMode currentMode = EYES;
unsigned long sensorTimer = 0;

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  dht.begin();
  timeClient.begin();
  updateWeatherData();

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2); 
  sensorTimer = millis();
}

void loop() {
  timeClient.update();
  unsigned long currentMillis = millis();

  // 定期抓天氣
  if (currentMillis - lastWeatherUpdate >= weatherInterval) {
    lastWeatherUpdate = currentMillis;
    updateWeatherData();
  }

  // TTP223 觸摸邏輯
  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) {
      touchStartTime = currentMillis;
      isTouching = true;
    }
    if (currentMillis - touchStartTime > longPressTime) {
      roboEyes.setMood(HAPPY);      
      manualMoodActive = true;
    }
  } else {
    if (isTouching) {
      if (currentMillis - touchStartTime < longPressTime) {
        currentMode = (currentMode == EYES) ? WEATHER : EYES;
      }
      manualMoodActive = false;
      isTouching = false;
    }
  }

  // 顯示切換
  if (currentMode == EYES) {
    handleEyeLogic(currentMillis);
    roboEyes.update(); 
  } else {
    refreshWeatherDisplay();
  }
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

void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  WiFiClient client;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&units=metric&cnt=25&appid=" + apiKey;//http://api.openweathermap.org/data/2.5/forecast?q=
  http.begin(client, url);
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
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

void refreshWeatherDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti = localtime(&rawtime);
  char dateBuf[12];
  sprintf(dateBuf, "%04d-%02d-%02d", ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
  
  display.setCursor(0, 0);
  display.print("Date: "); display.println(dateBuf);
  display.setCursor(0, 10);
  display.print("Time: "); display.println(timeClient.getFormattedTime());

  display.setCursor(0, 20);
  display.print("T: "); display.print(curTemp, 1);
  display.print("C  H: "); display.print(curHum); display.println("%");
  display.drawFastHLine(0, 30, 128, WHITE);

  display.setCursor(0, 34);
  display.println("Forecast:");
  display.setCursor(0, 44);
  display.print(forDate1); display.print(": "); 
  display.print(forTemp1, 0); display.print("C "); display.println(forWeather1);
  display.setCursor(0, 54);
  display.print(forDate2); display.print(": "); 
  display.print(forTemp2, 0); display.print("C "); display.println(forWeather2);
  
  display.display();
}
