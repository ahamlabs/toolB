# Makefile for toolB (Hot Reloading Enabled)

# Compiler
CC = gcc
GO = go

# --- Architecture-Aware Configuration for macOS ---
ARCH = $(shell uname -m)
ifeq ($(ARCH),arm64)
	HOMEBREW_PREFIX = /opt/homebrew
else
	HOMEBREW_PREFIX = /usr/local
endif
OPENSSL_DIR = $(HOMEBREW_PREFIX)/opt/openssl@3

# Flags
CFLAGS = -Wall -Isrc/include -O2 -pthread -I$(OPENSSL_DIR)/include
LDFLAGS = -L$(OPENSSL_DIR)/lib -lssl -lcrypto

# Directories
BIN_DIR = bin
APP_DIR = app
MONITOR_DIR = monitor

# Source files
C_SOURCES = src/heartware_server.c src/c_parser.c src/config.c src/ini.c
C_OBJS = $(C_SOURCES:.c=.o)
C_TARGET = $(BIN_DIR)/heartware_server
GO_TARGET = $(BIN_DIR)/monitor

# Default target
all: $(C_TARGET)

# Rule to link the C executable
$(C_TARGET): $(C_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $(C_TARGET) $(C_OBJS) $(CFLAGS) $(LDFLAGS)
	@echo "✅ toolB Concurrent Server compiled successfully for $(ARCH) -> $(C_TARGET)"

# Generic rule to compile any .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build the Go monitor
build-monitor:
	@echo "🛠️  Building Go monitor..."
	@cd $(MONITOR_DIR) && $(GO) build -o ../$(GO_TARGET) .
	@echo "✅ Go monitor compiled successfully -> $(GO_TARGET)"

# --- Target Commands ---
run-server: all
	@echo "🚀 Starting toolB C-Server..."
	@./$(C_TARGET)

# UPDATED: This target now runs the main.py script with the --reload flag
run-app:
	@echo "🐍 Starting Python Application Server with Hot Reloading..."
	@python3 $(APP_DIR)/main.py --reload

run-monitor: build-monitor
	@echo "📊 Starting Live Go Monitor..."
	@./$(GO_TARGET)

clean:
	@echo "🧹 Cleaning up..."
	@rm -f src/*.o
	@rm -rf $(BIN_DIR)
	@echo "Cleanup complete."

.PHONY: all run-server run-app clean run-monitor build-monitor
