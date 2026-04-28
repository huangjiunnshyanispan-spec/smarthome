CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Idriver -Iapp
LDFLAGS = -lasound

SRCS = driver/i2s_driver.c \
       driver/wav_parser.c \
       app/tts_engine.c    \
       app/main.c

TARGET = tts_player

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) /tmp/tts_output.wav

install-deps:
	sudo apt install -y libasound2-dev espeak-ng

.PHONY: all clean install-deps