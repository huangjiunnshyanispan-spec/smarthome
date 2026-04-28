#include "DHT.h"

#define DHTPIN 10     // 接在 GPIO 10
#define DHTTYPE DHT11 // 指定型號

// 關鍵修改：加入第三個參數 20，協助 ESP32-C3 處理時序
DHT dht(DHTPIN, DHTTYPE, 20); 

void setup() {
  // ESP32 建議使用 115200，若設 9600 請確保序列埠監控窗也是 9600
  Serial.begin(115200); 
  delay(1000);
  
  // 重要：在沒有外部電阻時，強制開啟內部上拉
  pinMode(DHTPIN, INPUT_PULLUP); 
  
  Serial.println(F("ESP32-C3 DHT11 強制讀取測試!"));
  dht.begin();
}

void loop() {
  delay(2500); // DHT11 讀取間隔建議大於 2 秒

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("讀取失敗！(原因：無外部電阻導致訊號微弱)"));
    return;
  }

  Serial.print(F("濕度: "));
  Serial.print(h);
  Serial.print(F("%  溫度: "));
  Serial.print(t);
  Serial.println(F("°C "));
}
