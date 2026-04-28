/*
 * ws2812b.h  -  WS2812B 8-LED driver for Raspberry Pi
 *
 * Uses: rpi_ws281x  (jgarff/rpi_ws281x) - supports Pi 3/4/5, Bookworm
 *
 * Install:
 *   git clone https://github.com/jgarff/rpi_ws281x
 *   cd rpi_ws281x && mkdir build && cd build
 *   cmake -D BUILD_SHARED=OFF .. && make
 *   sudo make install      # installs to /usr/local
 *
 * Wiring:
 *   WS2812B DIN  -->  GPIO 18  (Pin 12, PWM0)
 *   WS2812B 5V   -->  External 5V  (NOT Pi 5V pin!)
 *   WS2812B GND  -->  Pi GND + external GND (common ground)
 *
 * Compile: gcc -o ws2812b main.c ws2812b.c -lws2811 -lm
 * Run:     sudo ./ws2812b <mode>
 */
#ifndef WS2812B_H
#define WS2812B_H
#include <stdint.h>

/* ── User config ─────────────────────────── */
#define LED_COUNT       8
#define GPIO_PIN        10      /* 改為 10，對應實體引腳 Pin 19 (SPI0_MOSI) */
#define LED_BRIGHTNESS  160     /* Global scale 0~255              */
#define DMA_CHANNEL     10      /* 改為 10，SPI 模式建議使用 DMA 10          */

/* ── Colour ──────────────────────────────── */
typedef struct { uint8_t r, g, b; } rgb_t;

#define COLOR_OFF    ((rgb_t){  0,   0,   0})
#define COLOR_RED    ((rgb_t){200,   0,   0})
#define COLOR_GREEN  ((rgb_t){  0, 200,   0})
#define COLOR_WHITE  ((rgb_t){200, 200, 200})
#define COLOR_BLUE   ((rgb_t){  0,   0, 200})
#define COLOR_YELLOW ((rgb_t){200, 160,   0})

/* ── API ─────────────────────────────────── */
int  ws2812b_init(void);
void ws2812b_deinit(void);
void ws2812b_set(int index, rgb_t color);
void ws2812b_fill(rgb_t color);
void ws2812b_show(void);
void ws2812b_clear(void);

/* ── Modes (block until Ctrl-C) ─────────── */
void mode_off(void);
void mode_green(void);
void mode_red(void);
void mode_chase(rgb_t color, int tail, int delay_ms);

#endif /* WS2812B_H */
