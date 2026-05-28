CC = gcc
CFLAGS = -Wall -Wextra -O2
MACROS ?=
MHD_CFLAGS := $(shell pkg-config --cflags libmicrohttpd)
MHD_LIBS   := $(shell pkg-config --libs libmicrohttpd)
SSL_CFLAGS := $(shell pkg-config --cflags openssl)
SSL_LIBS   := $(shell pkg-config --libs openssl)

UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    SHARED_FLAG = -dynamiclib
else
    SHARED_FLAG = -shared
endif

$(LIB_TARGET): $(LIB_OBJ)
	@echo "Linking shared library -> $@"
	$(CC) $(SHARED_FLAG) -o $@ $^ $(SSL_LIBS)

SRC_DIR   = src
BUILD_DIR = build

LIB_SRC    = $(SRC_DIR)/main.c $(SRC_DIR)/rate_limiting.c $(SRC_DIR)/service.c
LIB_OBJ    = $(LIB_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
LIB_TARGET = $(BUILD_DIR)/rate_limiter.so

SERVER_SRC    = $(SRC_DIR)/endpoint.c $(SRC_DIR)/rate_limiting.c $(SRC_DIR)/service.c
SERVER_OBJ    = $(SERVER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SERVER_TARGET = $(BUILD_DIR)/server

.PHONY: all lib server clean run

all: $(LIB_TARGET) $(SERVER_TARGET)

lib: $(LIB_TARGET)

server: $(SERVER_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) $(MACROS) $(MHD_CFLAGS) $(SSL_CFLAGS) -fPIC -c $< -o $@

$(LIB_TARGET): $(LIB_OBJ)
	@echo "Linking shared library -> $@"
	$(CC) $(SHARED_FLAG) -o $@ $^ $(SSL_LIBS)

$(SERVER_TARGET): $(SERVER_OBJ)
	@echo "Linking server binary -> $@"
	$(CC) $(CFLAGS) $(MACROS) -o $@ $^ $(MHD_LIBS) $(SSL_LIBS)

run: $(SERVER_TARGET)
	./$(SERVER_TARGET)

clean:
	rm -rf $(BUILD_DIR)