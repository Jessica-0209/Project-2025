# Compiler and flags
CC = gcc
CFLAGS = -Wall -Iinclude -I/usr/include/cjson -I/usr/include/libnl3 -I$(HOSTAPD_DIR) -g

# Libraries (add mosquitto and cjson)
LIBS = -lmosquitto -lcjson -lpthread -lnl-3 -lnl-genl-3

# Directories
SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
BUILD_DIR = build
BIN_DIR = bin
HOSTAPD_DIR = ../hostap/hostapd

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(LIB_DIR)/*.c) 

# Object files in build directory
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

# Final executable
TARGET = $(BIN_DIR)/wifi_mqtt

.PHONY: all clean dirs

all: dirs $(TARGET) client_details ap_setup

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

# Compile CLI client separately (includes logging)
$(BIN_DIR)/client_details: tools/cli_client.c lib/log.c lib/network.c
	$(CC) $(CFLAGS) tools/cli_client.c lib/log.c lib/network.c -o $(BIN_DIR)/client_details $(LIBS)

client_details: $(BIN_DIR)/client_details

$(BIN_DIR)/ap_setup: tools/ap_setup.c lib/log.c lib/network.c lib/utils.c src/hostapd_listener.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

ap_setup: $(BIN_DIR)/ap_setup

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) $(BIN_DIR)/ap_setup $(BIN_DIR)/client_details
