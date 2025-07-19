# Makefile for toolB (Concurrent C-Parser Version)

# Compiler
CC = gcc

# Flags - Added -pthread for multi-threading
CFLAGS = -Wall -Isrc/include -O2 -pthread

# Directories
BIN_DIR = bin
APP_DIR = app

# Source files
C_SOURCES = src/heartware_server.c src/c_parser.c
C_OBJS = $(C_SOURCES:.c=.o)
TARGET = $(BIN_DIR)/heartware_server

# Default target
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(C_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $(TARGET) $(C_OBJS) $(CFLAGS)
	$(CC) -o $(TARGET) $(C_OBJS) $(CFLAGS) $(LIBS)
	@echo "✅ toolB Concurrent Server compiled successfully -> $(TARGET)"

# Generic rule to compile any .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Target Commands ---
run-server: all
	@echo "🚀 Starting toolB C-Server..."
	@./$(TARGET)

# UPDATED: This target now runs the new main.py application runner
run-app:
	@echo "🐍 Starting Python Application Server..."
	@python3 $(APP_DIR)/main.py

run-monitor:
	@echo "📊 Starting Live Monitor..."
	@python3 $(APP_DIR)/monitor.py

clean:
	@echo "🧹 Cleaning up..."
	@rm -f src/*.o
	@rm -rf $(BIN_DIR)
	@echo "Cleanup complete."

.PHONY: all run-server run-app clean run-monitor
