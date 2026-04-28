# 指定編譯器
CC = gcc

# 編譯參數
# -Wall: 顯示所有警告
# -O2: 最佳化
CFLAGS = -Wall -O2

# 連結函式庫
# -lmosquitto: MQTT 通訊
LIBS = -lmosquitto

# 執行檔名稱
TARGET = led_monitor

# 原始程式碼
SRCS = led_monitor.c

# 預設編譯動作
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)
	@echo "------------------------------------------------"
	@echo " 編譯成功！"
	@echo " 執行指令: sudo ./$(TARGET)"
	@echo "------------------------------------------------"

# 清除編譯檔案
clean:
	rm -f $(TARGET)
	@echo "已清除編譯檔。"

# 防止與同名檔案衝突
.PHONY: all clean
