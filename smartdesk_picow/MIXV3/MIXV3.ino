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
const char* ssid     = "iSpan-R309";
const char* password = "66316588";
String apiKey = "68c35f7a096fa892c724634f0dc30e0a";
String city   = "Taipei,TW";

#define DHTPIN   2
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

#define TOUCH_PIN    15
#define BUZZER_PIN   16
const int VOLUME       = 1;
const int longPressTime = 800;

// --- 全域變數 ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800);

enum DisplayMode { EYES, WEATHER, POMODORO };
DisplayMode currentMode = EYES;

float  curTemp    = 0.0;
int    curOutHum  = 0;
float  forTemp1   = 0.0, forTemp2 = 0.0, forTemp3 = 0.0;
String forDate1   = "--",  forDate2 = "--",  forDate3 = "--";
String forWeather1 = "--", forWeather2 = "--", forWeather3 = "--";

int  pMinutes = 25, pSeconds = 0;
bool isWorkMode     = true;
bool pomodoroActive = false;

unsigned long lastWeatherUpdate = 0;
unsigned long sensorTimer       = 0;
unsigned long pomodoroTimer     = 0;
unsigned long touchStartTime    = 0;
bool isTouching      = false;
bool manualMoodActive = false;

// --- 聲效 ---
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
void pauseSound() { playSoftTone(440, 80); playSoftTone(330,160); }

// ─────────────────────────────────────────────────────────────
//  天氣圖示  18×18 OLED 像素，傳入左上角 (x, y)
//  欄寬 42px，各欄中心：col0=21, col1=63, col2=105
//  圖示偏移使其水平置中：cx = colCenter - 9
// ─────────────────────────────────────────────────────────────
void drawWeatherIcon(int x, int y, String weather) {
  weather.toLowerCase();

  if (weather.indexOf("clear") >= 0 || weather.indexOf("sun") >= 0) {
    // 太陽：實心圓核心 + 8條光芒
    display.fillCircle(x+9, y+9, 4, WHITE);
    // 上下左右
    display.drawFastVLine(x+9, y,    3, WHITE);
    display.drawFastVLine(x+9, y+15, 3, WHITE);
    display.drawFastHLine(x,    y+9, 3, WHITE);
    display.drawFastHLine(x+15, y+9, 3, WHITE);
    // 斜向光芒
    display.drawLine(x+3, y+3,  x+4,  y+4,  WHITE);
    display.drawLine(x+14,y+3,  x+13, y+4,  WHITE);
    display.drawLine(x+3, y+14, x+4,  y+13, WHITE);
    display.drawLine(x+14,y+14, x+13, y+13, WHITE);

  } else if (weather.indexOf("cloud") >= 0) {
    // 雲：三圓弧組合底部平整
    display.fillCircle(x+5,  y+10, 5, WHITE);
    display.fillCircle(x+9,  y+7,  6, WHITE);
    display.fillCircle(x+14, y+10, 4, WHITE);
    // 底部矩形填平
    display.fillRect(x, y+10, 18, 5, WHITE);
    // 底線輪廓修整（黑色蓋掉多餘）
    display.drawFastHLine(x, y+14, 18, WHITE);

  } else if (weather.indexOf("rain") >= 0 || weather.indexOf("drizzle") >= 0) {
    // 雨：雲輪廓 + 四條斜雨線
    display.fillCircle(x+5,  y+7,  4, WHITE);
    display.fillCircle(x+9,  y+5,  5, WHITE);
    display.fillCircle(x+13, y+7,  3, WHITE);
    display.fillRect(x+1, y+7, 14, 4, WHITE);
    // 雨滴（斜線）
    display.drawLine(x+2,  y+12, x+1,  y+16, WHITE);
    display.drawLine(x+6,  y+12, x+5,  y+16, WHITE);
    display.drawLine(x+10, y+12, x+9,  y+16, WHITE);
    display.drawLine(x+14, y+12, x+13, y+16, WHITE);

  } else if (weather.indexOf("snow") >= 0) {
    // 雪花：米字形 + 端點小圓
    display.drawFastHLine(x+1, y+9,  16, WHITE);
    display.drawFastVLine(x+9, y+1,  16, WHITE);
    display.drawLine(x+3, y+3,  x+15, y+15, WHITE);
    display.drawLine(x+15,y+3,  x+3,  y+15, WHITE);
    display.fillCircle(x+1,  y+9,  1, WHITE);
    display.fillCircle(x+16, y+9,  1, WHITE);
    display.fillCircle(x+9,  y+1,  1, WHITE);
    display.fillCircle(x+9,  y+16, 1, WHITE);

  } else if (weather.indexOf("thunder") >= 0 || weather.indexOf("storm") >= 0) {
    // 雷雨：雲 + 閃電
    display.fillCircle(x+4,  y+6, 4, WHITE);
    display.fillCircle(x+9,  y+4, 5, WHITE);
    display.fillCircle(x+13, y+6, 3, WHITE);
    display.fillRect(x+1, y+6, 13, 3, WHITE);
    // 閃電（Z形）
    display.drawLine(x+9, y+10, x+6, y+13, WHITE);
    display.drawLine(x+6, y+13, x+10,y+13, WHITE);
    display.drawLine(x+10,y+13, x+7, y+17, WHITE);

  } else if (weather.indexOf("mist")  >= 0 ||
             weather.indexOf("fog")   >= 0 ||
             weather.indexOf("haze")  >= 0) {
    // 霧：四條長短交錯橫線
    display.drawFastHLine(x+1, y+3,  16, WHITE);
    display.drawFastHLine(x+3, y+7,  12, WHITE);
    display.drawFastHLine(x+1, y+11, 16, WHITE);
    display.drawFastHLine(x+3, y+15, 12, WHITE);

  } else {
    // 未知
    display.setCursor(x+5, y+5);
    display.setTextSize(1);
    display.print("?");
  }
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, INPUT);

  Wire.setSDA(4); Wire.setSCL(5);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for (;;); }

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

  if (currentMillis - lastWeatherUpdate >= 1800000) {
    lastWeatherUpdate = currentMillis;
    updateWeatherData();
  }

  if (pomodoroActive && (currentMillis - pomodoroTimer >= 1000)) {
    pomodoroTimer = currentMillis;
    runPomodoroLogic();
  }

  int touchState = digitalRead(TOUCH_PIN);
  if (touchState == HIGH) {
    if (!isTouching) { touchStartTime = currentMillis; isTouching = true; }
    if (currentMillis - touchStartTime > longPressTime) {
      if (currentMode == EYES) { roboEyes.setMood(HAPPY); manualMoodActive = true; }
    }
  } else {
    if (isTouching) {
      unsigned long duration = currentMillis - touchStartTime;
      if (duration < longPressTime) {
        if      (currentMode == EYES)    currentMode = WEATHER;
        else if (currentMode == WEATHER) currentMode = POMODORO;
        else                             currentMode = EYES;
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

  display.clearDisplay();
  if      (currentMode == EYES)     { handleEyeLogic(currentMillis); roboEyes.update(); }
  else if (currentMode == WEATHER)  { drawWeatherPage(); }
  else if (currentMode == POMODORO) { drawPomodoroPage(); }
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

// ─────────────────────────────────────────────────────────────
//  天氣頁面佈局（128×64）
//
//  y= 0  ┌─────────────────────────┐
//         │ 2025-06-01  12:34:56   │  ← textSize(1)，8px高
//  y=11   │ I:29C 72%  O:31C 68%  │  ← textSize(1)
//  y=22   ├──────┬────────┬────────┤
//         │ 06-02│ 06-03  │ 06-04  │  ← 日期 textSize(1)
//  y=32   │  ☁   │   ☀   │   🌧   │  ← 圖示 18×18px
//  y=50   │      │        │        │
//  y=55   │ 29C  │  32C   │  27C   │  ← 溫度 textSize(1)
//  y=63   └──────┴────────┴────────┘
// ─────────────────────────────────────────────────────────────
void drawWeatherPage() {
  display.setTextColor(WHITE);

  // ── Row 1：日期時間 ─────────────────────────────────────
  time_t rawtime = timeClient.getEpochTime();
  struct tm* ti  = localtime(&rawtime);
  char topBuf[22];
  sprintf(topBuf, "%04d-%02d-%02d %02d:%02d:%02d",
          ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday,
          ti->tm_hour, ti->tm_min, ti->tm_sec);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(topBuf);

  // ── Row 2：室內 / 室外溫度（整數）────────────────────────
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

  // ── 三日預報（三欄平均，各寬 42px）────────────────────────
  // 欄中心 x：col0=21, col1=63, col2=105
  // 圖示左上角 x = 欄中心 - 9（18px 圖示置中）
  const int colCenter[3] = {21, 63, 105};
  String dates[3]   = {forDate1,    forDate2,    forDate3};
  int    temps[3]   = {(int)round(forTemp1), (int)round(forTemp2), (int)round(forTemp3)};
  String weathrs[3] = {forWeather1, forWeather2, forWeather3};

  for (int i = 0; i < 3; i++) {
    int cx = colCenter[i];

    // 日期（5字元 MM-DD，置中）
    display.setTextSize(1);
    display.setCursor(cx - 20, 22);   // 5字 × 6px = 30px，偏移15px置中 → -15... 用-20留少許左空
    display.setCursor(cx - 14, 22);   // textSize(1)每字6px，5字=30px，起點=cx-15
    display.print(dates[i]);

    // 圖示 18×18，水平置中
    drawWeatherIcon(cx - 9, 32, weathrs[i]);

    // 溫度整數，置中
    // 最多3字元（如-9C）= 18px，起點=cx-9
    display.setCursor(cx - 9, 55);
    if (temps[i] >= 0 && temps[i] < 10) display.setCursor(cx - 6, 55);  // 個位數微調
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

void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  WiFiClient client;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city +
               "&units=metric&cnt=25&appid=" + apiKey;
  http.begin(client, url);
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());

    curTemp   = doc["list"][0]["main"]["temp"];
    curOutHum = doc["list"][0]["main"]["humidity"];

    forDate1    = doc["list"][8]["dt_txt"].as<String>().substring(5,10);
    forTemp1    = doc["list"][8]["main"]["temp"];
    forWeather1 = doc["list"][8]["weather"][0]["main"].as<String>();

    forDate2    = doc["list"][16]["dt_txt"].as<String>().substring(5,10);
    forTemp2    = doc["list"][16]["main"]["temp"];
    forWeather2 = doc["list"][16]["weather"][0]["main"].as<String>();

    forDate3    = doc["list"][24]["dt_txt"].as<String>().substring(5,10);
    forTemp3    = doc["list"][24]["main"]["temp"];
    forWeather3 = doc["list"][24]["weather"][0]["main"].as<String>();
  }
  http.end();
}