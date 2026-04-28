#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

/*
 * ws2812b.c  -  WS2812B driver wrapping rpi_ws281x
 *
 * rpi_ws281x handles:
 *   - Pi 3 (BCM2837) / Pi 4 (BCM2711) / Pi 5 register differences
 *   - /dev/mem mmap alignment
 *   - PWM + DMA waveform generation
 * We just set colours and call ws2811_render().
 */

#include "ws2812b.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ws2811/ws2811.h>   /* from rpi_ws281x */

/* ── rpi_ws281x channel config ───────────── */
static ws2811_t ledstring = {
    .freq   = WS2811_TARGET_FREQ,   /* 800000 Hz */
    .dmanum = DMA_CHANNEL,
    .channel = {
        [0] = {
            .gpionum    = GPIO_PIN,
            .invert     = 0,
            .count      = LED_COUNT,
            .strip_type = WS2811_STRIP_GRB,
            .brightness = LED_BRIGHTNESS,
        },
        [1] = { .gpionum = 0, .invert = 0, .count = 0, .brightness = 0 },
    },
};

static rgb_t led_buf[LED_COUNT];
static volatile int running = 1;

/* ── Signal handler ──────────────────────── */
static void sig_handler(int sig) { (void)sig; running = 0; }
static void setup_signal(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ── Init / deinit ───────────────────────── */
int ws2812b_init(void)
{
    ws2811_return_t ret = ws2811_init(&ledstring);
    if (ret != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        fprintf(stderr, "Make sure you are running with sudo.\n");
        return -1;
    }
    memset(led_buf, 0, sizeof(led_buf));
    printf("[ws2812b] init ok -- LED_COUNT=%d GPIO=%d DMA=%d\n",
           LED_COUNT, GPIO_PIN, DMA_CHANNEL);
    return 0;
}

void ws2812b_deinit(void)
{
    ws2812b_clear();
    ws2811_fini(&ledstring);
    printf("[ws2812b] finished\n");
}

/* ── Buffer helpers ──────────────────────── */
void ws2812b_set(int index, rgb_t color)
{
    if (index < 0 || index >= LED_COUNT) return;
    led_buf[index] = color;
}

void ws2812b_fill(rgb_t color)
{
    for (int i = 0; i < LED_COUNT; i++)
        led_buf[i] = color;
}

/* ── Render ──────────────────────────────── */
void ws2812b_show(void)
{
    /* Copy rgb_t buffer into rpi_ws281x channel (it uses 0x00RRGGBB format) */
    for (int i = 0; i < LED_COUNT; i++) {
        ledstring.channel[0].leds[i] =
            ((uint32_t)led_buf[i].r << 16) |
            ((uint32_t)led_buf[i].g <<  8) |
             (uint32_t)led_buf[i].b;
    }

    ws2811_return_t ret = ws2811_render(&ledstring);
    if (ret != WS2811_SUCCESS)
        fprintf(stderr, "ws2811_render error: %s\n", ws2811_get_return_t_str(ret));
}

void ws2812b_clear(void)
{
    ws2812b_fill(COLOR_OFF);
    ws2812b_show();
}

/* ── Light modes ─────────────────────────── */
void mode_green(void)
{
    setup_signal();
    running = 1;
    ws2812b_fill(COLOR_GREEN);
    ws2812b_show();
    printf("[GREEN] on  -- Ctrl-C to exit\n");
    // while(running)
    sleep(1);
    ws2812b_clear();
}

void mode_red(void)
{
    setup_signal();
    running = 1;
    ws2812b_fill(COLOR_RED);
    ws2812b_show();
    printf("[RED] on  -- Ctrl-C to exit\n");
    // while(running)
    sleep(1);
    ws2812b_clear();
}

void mode_chase(rgb_t color, int tail, int delay_ms)
{
    setup_signal();
    running = 1;
    printf("[CHASE] running for 20 seconds -- Ctrl-C to stop early\n");

    // 1. 紀錄開始時間
    time_t start_time = time(NULL);

    struct timespec ts = {
        .tv_sec  = delay_ms / 1000,
        .tv_nsec = (delay_ms % 1000) * 1000000L,
    };
    int pos = 0;

    // 2. 在迴圈條件中加入時間判斷
    while (running) {
        // 檢查是否已過 20 秒
        if (difftime(time(NULL), start_time) >= 20) {
            printf("[CHASE] 20 seconds timeout reached. Auto-closing...\n");
            break; // 跳出迴圈
        }

        for (int i = 0; i < LED_COUNT; i++) {
            int dist = ((pos - i) % LED_COUNT + LED_COUNT) % LED_COUNT;
            if (dist == 0) {
                ws2812b_set(i, color);
            } else if (dist <= tail) {
                float ratio = (float)(tail - dist + 1) / (float)(tail + 1);
                rgb_t dim = {
                    (uint8_t)(color.r * ratio),
                    (uint8_t)(color.g * ratio),
                    (uint8_t)(color.b * ratio),
                };
                ws2812b_set(i, dim);
            } else {
                ws2812b_set(i, COLOR_OFF);
            }
        }
        ws2812b_show();
        pos = (pos + 1) % LED_COUNT;
        nanosleep(&ts, NULL);
    }
    
    // 3. 退出迴圈後強制清空燈光
    ws2812b_clear();
}

void mode_off(void)
{
    ws2812b_clear();
    printf("[OFF] All LEDs turned off\n");
}
