#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// 1. 設定 WiFi 與 API
const char* ssid = "iSpan-R309";
const char* password = "66316588";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a"; 
String city = "Taipei,TW";

// 2. OLED 設定 (根據你的引腳設定)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 3. 時間與排程設定
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800); // UTC+8

unsigned long lastTimeRefresh = 0;
unsigned long lastWeatherUpdate = 0;
const long weatherInterval = 1800000; // 每 30 分鐘更新一次天氣 API

// 4. 天氣資訊全域變數 (用來儲存抓取到的資料)
float curTemp = 0.0, forTemp1 = 0.0, forTemp2 = 0.0;
int curHum = 0;
String forDate1 = "--", forDate2 = "--", forWeather1 = "--", forWeather2 = "--";

void setup() {
  Serial.begin(115200);
  
  // 初始化 I2C
  Wire.setSDA(4);
  Wire.setSCL(5);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.print(".");
    display.display();
  }

  timeClient.begin();
  updateWeatherData(); // 啟動時先抓一次天氣
}

void loop() {
  timeClient.update();
  unsigned long currentMillis = millis();

  // 每 500 毫秒重新繪製一次螢幕 (保持時間即時)
  if (currentMillis - lastTimeRefresh >= 500) {
    lastTimeRefresh = currentMillis;
    refreshDisplay();
  }

  // 定期抓取天氣資料 (不影響時間顯示)
  if (currentMillis - lastWeatherUpdate >= weatherInterval) {
    lastWeatherUpdate = currentMillis;
    updateWeatherData();
  }
}

// 負責從 API 抓取數據並存入變數
void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  WiFiClient client;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&units=metric&cnt=25&appid=" + apiKey;
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload);
    if (error) return;

    // 儲存現在天氣 (取 list 第一筆)
    curTemp = doc["list"][0]["main"]["temp"];
    curHum = doc["list"][0]["main"]["humidity"];

    // 儲存預報 (取第 8 筆與第 16 筆，約 24h 與 48h 後)
    forDate1 = doc["list"][8]["dt_txt"].as<String>().substring(5, 10);
    forTemp1 = doc["list"][8]["main"]["temp"];
    forWeather1 = doc["list"][8]["weather"][0]["main"].as<String>();

    forDate2 = doc["list"][16]["dt_txt"].as<String>().substring(5, 10);
    forTemp2 = doc["list"][16]["main"]["temp"];
    forWeather2 = doc["list"][16]["weather"][0]["main"].as<String>();
    
    Serial.println("Weather Updated!");
  }
  http.end();
}

// 負責把目前所有的資料畫在 OLED 上
void refreshDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // --- 1. 處理日期與即時時間 ---
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti = localtime(&rawtime);
  char dateBuf[12];
  sprintf(dateBuf, "%04d-%02d-%02d", ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
  
  display.setCursor(0, 0);
  display.print("Date: "); display.println(dateBuf);
  
  display.setCursor(0, 10);
  display.print("Time: "); 
  display.println(timeClient.getFormattedTime());

  // --- 2. 顯示即時天氣 (之前抓好的變數) ---
  display.setCursor(0, 20);
  display.print("T: "); display.print(curTemp, 1);
  display.print("C  H: "); display.print(curHum); display.println("%");

  display.drawFastHLine(0, 30, 128, WHITE);

  // --- 3. 預報部分 ---
  display.setCursor(0, 34);
  display.println("Forecast:");
  
  // 第一筆預報
  display.setCursor(0, 44);
  display.print(forDate1); display.print(": "); 
  display.print(forTemp1, 0); display.print("C "); display.println(forWeather1);

  // 第二筆預報
  display.setCursor(0, 54);
  display.print(forDate2); display.print(": "); 
  display.print(forTemp2, 0); display.print("C "); display.println(forWeather2);
  
  display.display();
}
