#ifndef TTS_ENGINE_H
#define TTS_ENGINE_H

typedef enum {
    TTS_BACKEND_ESPEAK = 0,
    TTS_BACKEND_PIPER,
    TTS_BACKEND_FESTIVAL
} TTSBackend;

typedef struct {
    TTSBackend  backend;
    char        language[16];
    char        model_path[256];   /* piper 模型路徑 */
    int         speed;
    int         pitch;
    float       volume;      /* 音量 0.0 ~ 1.0 */
} TTSEngine;

void tts_engine_init(TTSEngine *engine, TTSBackend backend,
                     const char *lang, const char *model_path,
                     int speed, int pitch);

int tts_engine_synthesize(TTSEngine *engine,
                           const char *text,
                           const char *output_path);

#endif