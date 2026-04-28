#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include "ws2812b.h"

/* ── 驅動私有變數 ── */
static int spi_fd = -1;
static uint32_t spi_speed = 6400000; // 6.4MHz 用於模擬 WS2812B 時序
static rgb_t led_buf[LED_COUNT];

/* WS2812B 協議編碼：用 SPI 1 Byte 模擬 WS2812B 1 Bit */
#define CODE_0 0xC0  // 11000000
#define CODE_1 0xF8  // 11111000

/* ── 初始化：接管核心設備節點 ── */
int ws2812b_init(void) {
    spi_fd = open("/dev/spidev0.0", O_RDWR);
    if (spi_fd < 0) {
        perror("[Driver] 無法開啟 SPI 設備，請檢查接線(Pin 19)與系統設定");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) return -1;
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) return -1;

    memset(led_buf, 0, sizeof(led_buf));
    printf("[Driver] 成功進入原始 SPI 驅動模式 (GPIO 10)\n");
    return 0;
}

void ws2812b_deinit(void) {
    ws2812b_clear();
    if (spi_fd >= 0) close(spi_fd);
}

/* ── 緩衝區操作 ── */
void ws2812b_set(int index, rgb_t color) {
    if (index >= 0 && index < LED_COUNT) led_buf[index] = color;
}

void ws2812b_fill(rgb_t color) {
    for (int i = 0; i < LED_COUNT; i++) led_buf[i] = color;
}

/* ── 核心技術：SPI 資料幀封裝 ── */
void ws2812b_show(void) {
    if (spi_fd < 0) return;

    size_t out_len = LED_COUNT * 3 * 8;
    uint8_t *tx_buf = malloc(out_len);
    if (!tx_buf) return;

    int ptr = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        // WS2812B 順序為 GRB
        uint32_t color = ((uint32_t)led_buf[i].g << 16) | 
                         ((uint32_t)led_buf[i].r << 8)  | 
                         (uint32_t)led_buf[i].b;
        for (int bit = 23; bit >= 0; bit--) {
            tx_buf[ptr++] = (color & (1 << bit)) ? CODE_1 : CODE_0;
        }
    }

    if (write(spi_fd, tx_buf, out_len) < 0) perror("[Driver] 傳輸失敗");
    free(tx_buf);
    
    // Reset 訊號
    usleep(50); 
}

void ws2812b_clear(void) {
    ws2812b_fill(COLOR_OFF);
    ws2812b_show();
}

/* ── 模式執行 ── */

void mode_off(void) {
    ws2812b_clear();
    printf("[Driver] LED 熄滅\n");
}

void mode_green(void) {
    ws2812b_fill(COLOR_GREEN);
    ws2812b_show();
    printf("[Driver] 顯示綠色 (維持 2 秒)\n");
    sleep(2); // 防止閃一下就消失
    ws2812b_clear();
}

void mode_red(void) {
    ws2812b_fill(COLOR_RED);
    ws2812b_show();
    printf("[Driver] 顯示紅色 (維持 2 秒)\n");
    sleep(2);
    ws2812b_clear();
}

void mode_chase(rgb_t color, int tail, int delay_ms) {
    time_t start_time = time(NULL);
    int pos = 0;

    printf("[Driver] 跑馬燈啟動 (20秒自動停止)\n");

    while (1) {
        if (difftime(time(NULL), start_time) >= 20) break;

        for (int i = 0; i < LED_COUNT; i++) {
            int dist = ((pos - i) % LED_COUNT + LED_COUNT) % LED_COUNT;
            if (dist == 0) {
                ws2812b_set(i, color);
            } else if (dist <= tail) {
                float ratio = (float)(tail - dist + 1) / (float)(tail + 1);
                rgb_t dim = { (uint8_t)(color.r * ratio), (uint8_t)(color.g * ratio), (uint8_t)(color.b * ratio) };
                ws2812b_set(i, dim);
            } else {
                ws2812b_set(i, COLOR_OFF);
            }
        }
        ws2812b_show();
        pos = (pos + 1) % LED_COUNT;
        usleep(delay_ms * 1000);
    }
    ws2812b_clear();
    printf("[Driver] 跑馬燈結束\n");
}
