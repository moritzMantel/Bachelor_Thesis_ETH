CC = gcc
CFLAGS = -Wall -Wextra -O2
MACROS ?=

MHD_CFLAGS := $(shell pkg-config --cflags libmicrohttpd)
MHD_LIBS   := $(shell pkg-config --libs libmicrohttpd)
SSL_CFLAGS := $(shell pkg-config --cflags openssl)
SSL_LIBS := $(shell pkg-config --libs openssl)

SRC_DIR   = src
BUILD_DIR = build

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET = $(BUILD_DIR)/endpoint

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) $(MACROS) $(MHD_CFLAGS) $(SSL_CFLAGS) -c $< -o $@

objects: $(OBJ)
	@echo "All object files built"

$(TARGET): $(OBJ)
	@echo "Linking $(OBJ) -> $@"
	$(CC) $(CFLAGS) $(MACROS) $(OBJ) -o $@ $(MHD_LIBS) $(SSL_LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)