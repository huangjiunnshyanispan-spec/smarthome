#include "wav_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* 讀取 little-endian 整數 */
static uint32_t read_le32(FILE *fp) {
    uint8_t b[4];
    fread(b, 1, 4, fp);
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) |
           ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static uint16_t read_le16(FILE *fp) {
    uint8_t b[2];
    fread(b, 1, 2, fp);
    return (uint16_t)b[0] | ((uint16_t)b[1]<<8);
}

int wav_parse_header(const char *path, WavInfo *info)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("[WAV] 開啟失敗"); return -1; }

    char tag[5] = {0};

    /* RIFF 標頭 */
    fread(tag, 1, 4, fp);
    if (memcmp(tag, "RIFF", 4)) {
        fprintf(stderr, "[WAV] 不是有效的 RIFF 檔案\n");
        fclose(fp); return -1;
    }
    read_le32(fp); /* 跳過 chunk size */

    fread(tag, 1, 4, fp);
    if (memcmp(tag, "WAVE", 4)) {
        fprintf(stderr, "[WAV] 不是 WAVE 格式\n");
        fclose(fp); return -1;
    }

    /* 搜尋 fmt chunk */
    while (1) {
        if (fread(tag, 1, 4, fp) != 4) { fclose(fp); return -1; }
        uint32_t chunk_size = read_le32(fp);

        if (!memcmp(tag, "fmt ", 4)) {
            read_le16(fp);                          /* audio format (PCM=1) */
            info->channels    = read_le16(fp);
            info->sample_rate = read_le32(fp);
            read_le32(fp);                          /* byte rate */
            read_le16(fp);                          /* block align */
            info->bit_depth   = read_le16(fp);
            /* 跳過額外欄位 */
            if (chunk_size > 16) fseek(fp, chunk_size - 16, SEEK_CUR);
        } else if (!memcmp(tag, "data", 4)) {
            info->data_size   = chunk_size;
            info->data_offset = (uint32_t)ftell(fp);
            break;
        } else {
            fseek(fp, chunk_size, SEEK_CUR);
        }
    }

    fclose(fp);
    return 0;
}

uint32_t wav_total_frames(const WavInfo *info)
{
    uint32_t bytes_per_frame = info->channels * (info->bit_depth / 8);
    return info->data_size / bytes_per_frame;
}