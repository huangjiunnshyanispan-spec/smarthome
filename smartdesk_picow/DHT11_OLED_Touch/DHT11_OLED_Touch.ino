#include "DHT.h"
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#define DHTPIN 2     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// TTP223 設定
#define TOUCH_PIN 15          // TTP223 接在 GP15
const int longPressTime = 800; // 長按定義為 800 毫秒
unsigned long touchStartTime = 0;
bool isTouching = false;
bool manualMoodActive = false; // 是否處於手動（長按）情緒狀態

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

unsigned long eventTimer; 

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT); // 初始化觸摸感測器

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }

  dht.begin();
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); 
  roboEyes.setPosition(DEFAULT); 
  roboEyes.setAutoblinker(ON, 3, 2); 
  display.setRotation(0); 

  eventTimer = millis(); 
}

void loop() {
  // 必須持續執行以驅動眼睛動畫
  roboEyes.update(); 

  // --- TTP223 觸摸偵測邏輯 ---
  int touchState = digitalRead(TOUCH_PIN);

  if (touchState == HIGH) {
    if (!isTouching) {
      touchStartTime = millis();
      isTouching = true;
    }
    // 持續按住超過 800ms 觸發開心表情
    if (millis() - touchStartTime > longPressTime) {
      roboEyes.setMood(HAPPY);      
      roboEyes.setSweat(OFF);       
      roboEyes.setHFlicker(OFF);    
      manualMoodActive = true;      
    }
  } else {
    // 手指一放開，立刻解除手動模式
    if (isTouching) {
      manualMoodActive = false;
      isTouching = false;
    }
  }

  // --- 溫度偵測邏輯 (每 5 秒檢查一次) ---
  if(millis() - eventTimer >= 5000){
    eventTimer = millis(); 
    
    float t = dht.readTemperature();
    if (isnan(t)) return;

    // 只有在「沒被按住」的情形下，才根據溫度更新心情
    if (!manualMoodActive) {
      if (t < 20){
        // 低溫：疲倦 + 閃爍
        roboEyes.setMood(TIRED);
        roboEyes.setHFlicker(ON, 2);
        roboEyes.setSweat(OFF);
        roboEyes.setPosition(DEFAULT);
      }
      else if (t > 25){
        // 高溫：疲倦 + 流汗 + 視線往下
        roboEyes.setMood(TIRED);
        roboEyes.setSweat(ON);
        roboEyes.setPosition(S);
        roboEyes.setHFlicker(OFF);
      }
      else {
        // 正常溫度：預設表情
        roboEyes.setMood(DEFAULT);
        roboEyes.setPosition(DEFAULT);
        roboEyes.setHFlicker(OFF);
        roboEyes.setSweat(OFF);
      }
    }
  }
}
