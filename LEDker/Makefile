CC      = gcc
# 移除 LDFLAGS 中的 -lws2811，因為我們現在直接操作系統底層設備
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lm

TARGET  = ws2812b
SRCS    = main.c ws2812b.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "-------------------------------------------------------"
	@echo " 編譯成功！目前模式：[Linux spidev 原始驅動模式]"
	@echo " 此模式不再依賴 rpi_ws281x 庫，直接控制核心設備節點。"
	@echo "-------------------------------------------------------"

%.o: %.c ws2812b.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
