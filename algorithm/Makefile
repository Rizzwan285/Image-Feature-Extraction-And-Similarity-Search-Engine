CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -Iinclude -pthread
LDFLAGS = -pthread -lm

SRC_DIR = src
BIN_DIR = bin

SRCS    = $(wildcard $(SRC_DIR)/*.c)
OBJS    = $(SRCS:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)
TARGET  = $(BIN_DIR)/imgsearch

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
