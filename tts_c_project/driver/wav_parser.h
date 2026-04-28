#ifndef WAV_PARSER_H
#define WAV_PARSER_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
    uint32_t data_offset;   /* 音訊資料在檔案中的起始位置 */
    uint32_t data_size;     /* 音訊資料的位元組總數 */
} WavInfo;

/**
 * 解析 WAV 檔案標頭
 * @param path  WAV 檔案路徑
 * @param info  輸出解析結果
 * @return 0 成功，負值為錯誤
 */
int wav_parse_header(const char *path, WavInfo *info);

/**
 * 從 WAV 解析結果計算總 frame 數
 */
uint32_t wav_total_frames(const WavInfo *info);

#endif