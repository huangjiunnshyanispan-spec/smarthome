#include "tts_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void tts_engine_init(TTSEngine *engine, TTSBackend backend,
                     const char *lang, const char *model_path,
                     int speed, int pitch)
{
    engine->backend = backend;
    strncpy(engine->language, lang, sizeof(engine->language) - 1);
    strncpy(engine->model_path, model_path ? model_path : "",
            sizeof(engine->model_path) - 1);
    engine->speed = speed;
    engine->pitch = pitch;
}

int tts_engine_synthesize(TTSEngine *engine,
                           const char *text,
                           const char *output_path)
{
    char cmd[2048];

    switch (engine->backend) {

        case TTS_BACKEND_PIPER:
              /* 合成 → 開頭加 0.3 秒靜音緩衝 → 調整音量 */
            snprintf(cmd, sizeof(cmd),
                "echo \"%s\" | python3 -m piper "
                "--model \"%s\" "
                "--output_file /tmp/tts_raw.wav 2>/dev/null && "
                "sox -n -r 22050 -c 1 -b 16 /tmp/tts_silence.wav trim 0.0 0.5 && "
                "sox /tmp/tts_silence.wav /tmp/tts_raw.wav \"%s\" vol %.2f",
                text,
                engine->model_path,
                output_path,
                engine->volume);
            break;
        case TTS_BACKEND_ESPEAK:
            snprintf(cmd, sizeof(cmd),
                     "espeak-ng -v %s -s %d -p %d -w \"%s\" \"%s\" 2>/dev/null",
                     engine->language,
                     engine->speed,
                     engine->pitch,
                     output_path,
                     text);
            break;

        case TTS_BACKEND_FESTIVAL:
            snprintf(cmd, sizeof(cmd),
                     "echo \"%s\" | text2wave -o \"%s\" 2>/dev/null",
                     text, output_path);
            break;

        default:
            fprintf(stderr, "[TTS] 未知 backend\n");
            return -1;
    }

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[TTS] 合成失敗，指令：%s\n", cmd);
        return -1;
    }
    return 0;
}