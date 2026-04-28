const int touchPin = 15;
unsigned long startTime = 0;
bool isPressing = false;
const int longPressDuration = 500; // 設定長按門檻（毫秒）

void setup() {
  pinMode(touchPin, INPUT);
  Serial.begin(115200);
}

void loop() {
  int touchState = digitalRead(touchPin);

  // 剛按下的瞬間
  if (touchState == HIGH && !isPressing) {
    isPressing = true;
    startTime = millis(); // 紀錄開始時間
  }

  // 放開的瞬間
  if (touchState == LOW && isPressing) {
    unsigned long duration = millis() - startTime; // 計算持續多久
    
    if (duration >= longPressDuration) {
      Serial.print("【長按】持續時間: ");
      Serial.println(duration);
    } else if (duration > 50) { // 加上 50ms 的防彈跳過濾
      Serial.print("【短按】持續時間: ");
      Serial.println(duration);
    }
    
    isPressing = false; // 重置狀態
  }
}
