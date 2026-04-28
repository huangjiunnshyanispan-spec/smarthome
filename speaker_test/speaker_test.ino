#include <I2S.h>

// 定義 Pico W 腳位 (GPIO 編號)
const int BCLK_PIN = 18;
const int LRCLK_PIN = 19;
const int DOUT_PIN = 20;

I2S i2sOutput(OUTPUT);

void setup() {
  // 設定 I2S 採樣率與位元深度
  i2sOutput.setBCLK(BCLK_PIN);
  i2sOutput.setDATA(DOUT_PIN);
  i2sOutput.begin(44100); // 44.1kHz
}

void loop() {
  // 產生簡單的方波雜音來測試是否有聲音
  int16_t sample = 10000; // 震幅
  for (int i = 0; i < 100; i++) {
    i2sOutput.write(sample); // 左聲道
    i2sOutput.write(sample); // 右聲道
  }
  sample = -10000;
  for (int i = 0; i < 100; i++) {
    i2sOutput.write(sample);
    i2sOutput.write(sample);
  }
}
