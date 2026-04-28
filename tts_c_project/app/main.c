#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tts_engine.h"
#include "../driver/i2s_driver.h"
#include "../driver/wav_parser.h"

#define TMP_WAV     "/tmp/tts_output.wav"
#define CONFIG_PATH "config/device.conf"
#define CHUNK_FRAMES 1024

/**
 * 應用層主流程：文字 → WAV → I2S → 喇叭
 */
static int speak(TTSEngine *tts, I2SDriver *drv, const char *text)
{
    WavInfo  wav;
    FILE    *fp = NULL;
    uint8_t *buf = NULL;
    int      ret = 0;

    printf("[App] 合成語音：%s\n", text);
    if (tts_engine_synthesize(tts, text, TMP_WAV) != 0) {
        fprintf(stderr, "[App] TTS 合成失敗\n");
        return -1;
    }

    /* 解析 WAV 實際格式 */
    if (wav_parse_header(TMP_WAV, &wav) != 0) {
        fprintf(stderr, "[App] 解析 WAV 失敗\n");
        return -1;
    }

    printf("[App] WAV 格式：%u Hz / %dch / %d-bit\n",
           wav.sample_rate, wav.channels, wav.bit_depth);

    /* ★ 用 WAV 實際格式重新初始化驅動 */
    i2s_driver_close(drv);

    I2SConfig new_cfg;
    i2s_config_load("config/device.conf", &new_cfg);
    new_cfg.sample_rate = wav.sample_rate;   /* 以 WAV 為準 */
    new_cfg.channels    = wav.channels;
    new_cfg.bit_depth   = wav.bit_depth;

    if (i2s_driver_init(drv, &new_cfg) != 0) {
        fprintf(stderr, "[App] 驅動重新初始化失敗\n");
        return -1;
    }

    uint32_t bytes_per_frame = wav.channels * (wav.bit_depth / 8);
    uint32_t chunk_bytes     = 1024 * bytes_per_frame;

    fp = fopen(TMP_WAV, "rb");
    if (!fp) { perror("[App] 開啟 WAV 失敗"); return -1; }
    fseek(fp, wav.data_offset, SEEK_SET);

    buf = malloc(chunk_bytes);
    if (!buf) { fclose(fp); return -ENOMEM; }

    size_t n;
    while ((n = fread(buf, 1, chunk_bytes, fp)) > 0) {
        snd_pcm_uframes_t frames = n / bytes_per_frame;
        snd_pcm_sframes_t written = i2s_driver_write(drv, buf, frames);
        if (written < 0) {
            fprintf(stderr, "[App] 寫入 I2S 失敗: %ld\n", written);
            ret = -1;
            break;
        }
    }

    i2s_driver_drain(drv);
    printf("[App] 播放完成\n");

    if (buf) free(buf);
    if (fp)  fclose(fp);
    unlink(TMP_WAV);
    return ret;
}

int main(int argc, char *argv[])
{
    I2SConfig config;
    I2SDriver driver;
    TTSEngine tts;
    int       ret = 0;

    /* 載入裝置設定（失敗時用預設值） */
    i2s_config_load(CONFIG_PATH, &config);

    /* 初始化驅動層 */
    if (i2s_driver_init(&driver, &config) != 0) {
        fprintf(stderr, "[Main] 驅動初始化失敗，請確認 ALSA 設定\n");
        return 1;
    }

    /* 初始化應用層 TTS */
    tts_engine_init(&tts, TTS_BACKEND_PIPER, "zh","/root/zh_CN-huayan-medium.onnx", 150, 50);
    tts.volume = 0.1f;
    if (argc > 1) {
        /* 命令列模式：將所有參數合併為一段文字 */
        char text[1024] = {0};
        for (int i = 1; i < argc; i++) {
            strncat(text, argv[i], sizeof(text) - strlen(text) - 2);
            if (i < argc - 1) strncat(text, " ", 2);
        }
        ret = speak(&tts, &driver, text);
    } else {
        /* 互動模式 */
        char text[1024];
        printf("TTS 播放器（輸入 quit 離開）\n");
        while (1) {
            printf("> ");
            fflush(stdout);
            if (!fgets(text, sizeof(text), stdin)) break;
            text[strcspn(text, "\n")] = '\0';   /* 去掉換行 */
            if (!strcmp(text, "quit")) break;
            if (strlen(text) > 0)
                speak(&tts, &driver, text);
        }
    }

    i2s_driver_close(&driver);
    return ret;
}