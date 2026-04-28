#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mosquitto.h>

// MQTT 訊息回呼：收到訊息直接呼叫驅動
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (!msg->payload) return;
    char *payload = (char *)msg->payload;
    char cmd[64];

    if (strcmp(payload, "online") == 0) {
        sprintf(cmd, "sudo /home/pi/LEDker/ws2812b green &");
        system(cmd);
    } 
    else if (strcmp(payload, "offline") == 0) {
        sprintf(cmd, "sudo /home/pi/LEDker/ws2812b red &");
        system(cmd);
    }
}

// 處理 Ctrl+C 退出
void handle_sigint(int sig) {
    system("sudo /home/pi/LEDker/ws2812b off > /dev/null 2>&1");
    exit(0);
}

int main() {
    struct mosquitto *mosq;

    mosquitto_lib_init();
    mosq = mosquitto_new("led_simple_bridge", true, NULL);
    
    // 設定回呼與訊號處理
    mosquitto_message_callback_set(mosq, on_message);
    signal(SIGINT, handle_sigint);

    // 連接本地 Aedes Broker
    if (mosquitto_connect(mosq, "127.0.0.1", 1883, 60) != MOSQ_ERR_SUCCESS) return 1;

    // 訂閱主題並開始循環
    mosquitto_subscribe(mosq, NULL, "home/+/status", 0);
    
    printf("LED 簡潔監聽器運行中...\n");
    mosquitto_loop_forever(mosq, -1, 1);

    return 0;
}
