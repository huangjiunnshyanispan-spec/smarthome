#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 硬體腳位設定
const int buzzerPin = 16; // Pin 21 (GP16)
const int VOLUME = 1;     // 音量設為最小 (1/256 佔空比)

// 計時變數
unsigned long previousMillis = 0;
int minutes = 25;
int seconds = 0;
bool isWorkMode = true;

void setup() {
  pinMode(buzzerPin, OUTPUT);
  analogWrite(buzzerPin, 0); // 初始靜音
  
  Wire.setSDA(4); 
  Wire.setSCL(5); 
  Wire.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }
  
  display.clearDisplay();
  startSound(); // 測試開機音量
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    
    if (seconds == 0) {
      if (minutes == 0) {
        isWorkMode = !isWorkMode;
        minutes = isWorkMode ? 25 : 5;
        seconds = 0;
        if (isWorkMode) startSound(); else restSound();
      } else {
        minutes--;
        seconds = 59;
      }
    } else {
      seconds--;
    }
    updateDisplay();
  }
}

// 軟體調音函式：手動控制頻率與極低佔空比
void playSoftTone(int freq, int duration) {
  analogWriteFreq(freq);      // 設定頻率
  analogWrite(buzzerPin, VOLUME); // 設定極小音量
  delay(duration);            // 持續時間
  analogWrite(buzzerPin, 0);  // 停止發聲
}

void startSound() {
  playSoftTone(523, 100); // Do
  playSoftTone(659, 100); // Mi
  playSoftTone(784, 200); // Sol
}

void restSound() {
  playSoftTone(784, 100); // Sol
  playSoftTone(659, 100); // Mi
  playSoftTone(523, 200); // Do
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 0);
  display.println(isWorkMode ? ">>FOCUS<<" : "==REST==");
  
  display.setTextSize(3);
  display.setCursor(20, 30);
  if (minutes < 10) display.print("0");
  display.print(minutes);
  display.print(":");
  if (seconds < 10) display.print("0");
  display.print(seconds);
  display.display();
}
