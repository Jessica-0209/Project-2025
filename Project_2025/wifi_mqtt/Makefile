# Compiler and flags
CC = gcc
CFLAGS = -Wall -Iinc -I/usr/include/cjson -g

# Libraries (add mosquitto and cjson)
LIBS = -lmosquitto -lcjson -lpthread

# Directories
SRC_DIR = src
INC_DIR = inc
LIB_DIR = lib
BUILD_DIR = build
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(LIB_DIR)/*.c)

# Object files in build directory
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

# Final executable
TARGET = $(BIN_DIR)/wifi_mqtt

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Compile each .c into .o with includes
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link objects into executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile CLI client separately
$(BIN_DIR)/cli_client: tools/cli_client.c
	$(CC) $(CFLAGS) tools/cli_client.c -o $(BIN_DIR)/cli_client $(LIBS)

cli_client: $(BIN_DIR)/cli_client

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

