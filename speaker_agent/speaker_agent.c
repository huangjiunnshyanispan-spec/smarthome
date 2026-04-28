#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>

#define SUB_TOPIC "home/speaker/say"
// 定義 TTS 程式的絕對路徑
#define TTS_PLAYER_PATH "/home/pi/tts_c_project/tts_player"

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (msg->payloadlen > 0) {
        char voice_text[512] = {0};
        int len = (msg->payloadlen < 511) ? msg->payloadlen : 511;
        memcpy(voice_text, msg->payload, len);

        printf("收到播報請求: [%s]\n", voice_text);

        // 使用絕對路徑構建指令，確保萬無一失
        char command[1024];
        snprintf(command, sizeof(command), "%s \"%s\"", TTS_PLAYER_PATH, voice_text);

        printf("執行指令: %s\n", command);
        
        int ret = system(command);
        if (ret != 0) {
            fprintf(stderr, "執行失敗，請檢查 %s 是否具備執行權限\n", TTS_PLAYER_PATH);
        }
    }
}

int main() {
    struct mosquitto *mosq = NULL;
    mosquitto_lib_init();

    mosq = mosquitto_new("RPi_Speaker_Agent", true, NULL);
    if (!mosq) return 1;

    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, "localhost", 1883, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "無法連接 Broker\n");
        return 1;
    }

    mosquitto_subscribe(mosq, NULL, SUB_TOPIC, 0);
    printf("語音代理程式已啟動 (路徑: %s)\n", TTS_PLAYER_PATH);

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
