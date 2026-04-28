#include "i2s_driver.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ── 內部輔助：設定 ALSA 硬體參數 ── */
static int _set_hw_params(I2SDriver *drv)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t     fmt;
    int err;

    /* 選擇位元深度格式 */
    switch (drv->config.bit_depth) {
        case 16: fmt = SND_PCM_FORMAT_S16_LE; break;
        case 24: fmt = SND_PCM_FORMAT_S24_LE; break;
        case 32: fmt = SND_PCM_FORMAT_S32_LE; break;
        default:
            fprintf(stderr, "[Driver] 不支援的位元深度: %d\n",
                    drv->config.bit_depth);
            return -EINVAL;
    }

    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(drv->pcm_handle, params)) < 0)
        return err;

    /* 存取模式：交錯式（左右聲道交替） */
    if ((err = snd_pcm_hw_params_set_access(
            drv->pcm_handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        return err;

    if ((err = snd_pcm_hw_params_set_format(
            drv->pcm_handle, params, fmt)) < 0)
        return err;

    if ((err = snd_pcm_hw_params_set_channels(
            drv->pcm_handle, params, drv->config.channels)) < 0)
        return err;

    unsigned int rate = drv->config.sample_rate;
    if ((err = snd_pcm_hw_params_set_rate_near(
            drv->pcm_handle, params, &rate, 0)) < 0)
        return err;
    if (rate != drv->config.sample_rate) {
        fprintf(stderr, "[Driver] 警告：取樣率調整為 %u Hz\n", rate);
        drv->config.sample_rate = rate;
    }

    snd_pcm_uframes_t buf = drv->config.buffer_size;
    snd_pcm_uframes_t per = drv->config.period_size;

    if ((err = snd_pcm_hw_params_set_buffer_size_near(
            drv->pcm_handle, params, &buf)) < 0)
        return err;

    if ((err = snd_pcm_hw_params_set_period_size_near(
            drv->pcm_handle, params, &per, NULL)) < 0)
        return err;

    if ((err = snd_pcm_hw_params(drv->pcm_handle, params)) < 0)
        return err;

    fprintf(stderr, "[Driver] ALSA 設定完成：%u Hz / %dch / %d-bit\n",
            drv->config.sample_rate,
            drv->config.channels,
            drv->config.bit_depth);
    return 0;
}

/* ── 公開 API 實作 ── */

int i2s_driver_init(I2SDriver *driver, const I2SConfig *config)
{
    int err;

    if (!driver || !config) return -EINVAL;

    memset(driver, 0, sizeof(I2SDriver));
    memcpy(&driver->config, config, sizeof(I2SConfig));

    /* 開啟 ALSA PCM 裝置（播放方向） */
    err = snd_pcm_open(&driver->pcm_handle,
                       config->device_name,
                       SND_PCM_STREAM_PLAYBACK,
                       0);
    if (err < 0) {
        i2s_driver_perror("無法開啟 ALSA 裝置", err);
        return err;
    }

    /* 設定硬體參數 */
    err = _set_hw_params(driver);
    if (err < 0) {
        i2s_driver_perror("設定 HW 參數失敗", err);
        snd_pcm_close(driver->pcm_handle);
        return err;
    }

    driver->is_open = 1;
    return 0;
}

snd_pcm_sframes_t i2s_driver_write(I2SDriver *driver,
                                    const void *data,
                                    snd_pcm_uframes_t frames)
{
    snd_pcm_sframes_t written;

    if (!driver || !driver->is_open) return -EBADF;

    written = snd_pcm_writei(driver->pcm_handle, data, frames);

    /* 處理 underrun（EPIPE） */
    if (written == -EPIPE) {
        fprintf(stderr, "[Driver] Underrun 發生，嘗試恢復...\n");
        snd_pcm_prepare(driver->pcm_handle);
        written = snd_pcm_writei(driver->pcm_handle, data, frames);
    }

    return written;
}

int i2s_driver_drain(I2SDriver *driver)
{
    if (!driver || !driver->is_open) return -EBADF;
    return snd_pcm_drain(driver->pcm_handle);
}

void i2s_driver_close(I2SDriver *driver)
{
    if (!driver) return;
    if (driver->is_open && driver->pcm_handle) {
        snd_pcm_drain(driver->pcm_handle);
        snd_pcm_close(driver->pcm_handle);
        driver->pcm_handle = NULL;
        driver->is_open = 0;
    }
}

int i2s_config_load(const char *path, I2SConfig *config)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[Driver] 無法開啟設定檔");
        /* 套用預設值 */
        strncpy(config->device_name, "hw:0,0", sizeof(config->device_name));
        config->sample_rate = 44100;
        config->channels    = 1;
        config->bit_depth   = 16;
        config->buffer_size = 4096;
        config->period_size = 1024;
        return -1;
    }

    char key[64], val[128];
    while (fscanf(fp, "%63s = %127s", key, val) == 2) {
        if      (!strcmp(key, "device"))      strncpy(config->device_name, val, 64);
        else if (!strcmp(key, "sample_rate")) config->sample_rate = (uint32_t)atoi(val);
        else if (!strcmp(key, "channels"))    config->channels    = (uint8_t)atoi(val);
        else if (!strcmp(key, "bit_depth"))   config->bit_depth   = (uint8_t)atoi(val);
        else if (!strcmp(key, "buffer_size")) config->buffer_size = (uint32_t)atoi(val);
        else if (!strcmp(key, "period_size")) config->period_size = (uint32_t)atoi(val);
    }
    fclose(fp);
    return 0;
}

void i2s_driver_perror(const char *msg, int err)
{
    fprintf(stderr, "[Driver] %s: %s\n", msg, snd_strerror(err));
}