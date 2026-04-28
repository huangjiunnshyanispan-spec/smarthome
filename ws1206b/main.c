/*
 * main.c  -  WS2812B control entry point
 *
 * sudo ./ws2812b green
 * sudo ./ws2812b red
 * sudo ./ws2812b chase [--color white|red|green|blue|yellow] [--tail N] [--delay ms]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ws2812b.h"

static void usage(const char *p)
{
    printf("Usage: sudo %s <mode> [options]\n\n", p);
    printf("  off                     Turn off all LEDs\n");
    printf("  green                   All LEDs solid green\n");
    printf("  red                     All LEDs solid red\n");
    printf("  chase                   Running-light animation\n\n");
    printf("  Chase options:\n");
    printf("    --color  white|red|green|blue|yellow  (default: white)\n");
    printf("    --tail   1~%d                           (default: 3)\n", LED_COUNT-1);
    printf("    --delay  ms                            (default: 80)\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }
    if (ws2812b_init() < 0) return 1;

    const char *mode = argv[1];

    if (!strcmp(mode, "off")) {
        mode_off(); // 執行關燈

    } else if (!strcmp(mode, "green")) {
        mode_green();

    } else if (!strcmp(mode, "red")) {
        mode_red();

    } else if (!strcmp(mode, "chase")) {
        rgb_t color = COLOR_WHITE;
        int tail = 3, delay = 80;

        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--color") && i+1 < argc) {
                const char *c = argv[++i];
                if      (!strcmp(c,"red"))    color = COLOR_RED;
                else if (!strcmp(c,"green"))  color = COLOR_GREEN;
                else if (!strcmp(c,"blue"))   color = COLOR_BLUE;
                else if (!strcmp(c,"yellow")) color = COLOR_YELLOW;
                else if (!strcmp(c,"white"))  color = COLOR_WHITE;
                else { fprintf(stderr,"Unknown color: %s\n",c); goto fail; }
            } else if (!strcmp(argv[i],"--tail") && i+1 < argc) {
                tail = atoi(argv[++i]);
                if (tail < 1 || tail >= LED_COUNT) {
                    fprintf(stderr,"--tail must be 1~%d\n", LED_COUNT-1); goto fail;
                }
            } else if (!strcmp(argv[i],"--delay") && i+1 < argc) {
                delay = atoi(argv[++i]);
                if (delay < 1 || delay > 5000) {
                    fprintf(stderr,"--delay must be 1~5000\n"); goto fail;
                }
            } else {
                fprintf(stderr,"Unknown option: %s\n", argv[i]); goto fail;
            }
        }
        mode_chase(color, tail, delay);

    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode); goto fail;
    }

    ws2812b_deinit();
    return 0;
fail:
    ws2812b_deinit();
    return 1;
}
