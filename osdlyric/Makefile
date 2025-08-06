# OSD Lyrics Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = `pkg-config --libs gtk+-3.0` -lcurl -ljson-c
INCLUDES = `pkg-config --cflags gtk+-3.0`

TARGET = osd_lyrics
TEST_TARGET = test_lyrics
LIB_SOURCES = osd_lyrics_lib.c
MAIN_SOURCES = osd_lyrics.c
TEST_SOURCES = test_lyrics.c
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)
MAIN_OBJECTS = $(MAIN_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

.PHONY: all clean install uninstall test

all: $(TARGET)

test: $(TEST_TARGET)

$(TARGET): $(MAIN_OBJECTS) $(LIB_OBJECTS)
	$(CC) $(MAIN_OBJECTS) $(LIB_OBJECTS) -o $(TARGET) $(LIBS)

$(TEST_TARGET): $(TEST_OBJECTS) $(LIB_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LIB_OBJECTS) -o $(TEST_TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(MAIN_OBJECTS) $(LIB_OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# 开发用的调试版本
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# 检查依赖
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists gtk+-3.0 || (echo "GTK+3.0 development libraries not found. Please install libgtk-3-dev" && exit 1)
	@echo "All dependencies satisfied."

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 运行测试程序
run-test: $(TEST_TARGET)
	./$(TEST_TARGET) -t

# 帮助信息
help:
	@echo "Available targets:"
	@echo "  all        - Build the program (default)"
	@echo "  clean      - Remove built files"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  uninstall  - Remove from /usr/local/bin"
	@echo "  debug      - Build debug version"
	@echo "  check-deps - Check if dependencies are installed"
	@echo "  run        - Build and run the program"
	@echo "  test       - Build the test program"
	@echo "  run-test   - Build and run the test program"
	@echo "  help       - Show this help message"
