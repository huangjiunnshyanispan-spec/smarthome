#ifndef I2S_DRIVER_H
#define I2S_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <alsa/asoundlib.h>

/* 裝置設定結構 */
typedef struct {
    char        device_name[64];  /* ALSA 裝置名稱，例如 "hw:0,0" */
    uint32_t    sample_rate;      /* 取樣率，例如 44100 */
    uint8_t     channels;         /* 聲道數，1=mono, 2=stereo */
    uint8_t     bit_depth;        /* 位元深度，16 或 24 */
    uint32_t    buffer_size;      /* 緩衝區大小（frames） */
    uint32_t    period_size;      /* 週期大小（frames） */
} I2SConfig;

/* 驅動控制代碼 */
typedef struct {
    snd_pcm_t  *pcm_handle;
    I2SConfig   config;
    int         is_open;
} I2SDriver;

/* --- 驅動層公開 API --- */

/**
 * 初始化 I2S 驅動，讀取設定並開啟 ALSA 裝置
 * @param driver   驅動控制代碼（由呼叫方分配）
 * @param config   裝置設定
 * @return 0 成功，負值為錯誤碼
 */
int  i2s_driver_init(I2SDriver *driver, const I2SConfig *config);

/**
 * 將 PCM 資料寫入 I2S 輸出
 * @param driver   已初始化的驅動
 * @param data     PCM 原始資料指標
 * @param frames   要寫入的 frame 數
 * @return 實際寫入的 frame 數，負值為錯誤
 */
snd_pcm_sframes_t i2s_driver_write(I2SDriver *driver,
                                    const void *data,
                                    snd_pcm_uframes_t frames);

/**
 * 等待目前播放完成（drain）
 */
int  i2s_driver_drain(I2SDriver *driver);

/**
 * 關閉驅動並釋放所有資源
 */
void i2s_driver_close(I2SDriver *driver);

/**
 * 從設定檔載入 I2SConfig
 * @param path    設定檔路徑
 * @param config  輸出設定結構
 * @return 0 成功
 */
int  i2s_config_load(const char *path, I2SConfig *config);

/**
 * 印出驅動錯誤（包含 ALSA 錯誤字串）
 */
void i2s_driver_perror(const char *msg, int err);

#endif /* I2S_DRIVER_H */