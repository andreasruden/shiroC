# Directories
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
SRC_DIR = src

# Tools & Flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -pedantic -I$(SRC_DIR)
DEBUGFLAGS = -g -O0
LD = ld

# Common source files
COMMON_SRCS = \
    $(SRC_DIR)/lexer.c \
    $(SRC_DIR)/parser.c \
    $(SRC_DIR)/ast/node.c \
	$(SRC_DIR)/ast/root.c \
	$(SRC_DIR)/ast/def/def.c \
    $(SRC_DIR)/ast/def/fn_def.c \
	$(SRC_DIR)/ast/expr/expr.c \
	$(SRC_DIR)/ast/expr/int_lit.c \
	$(SRC_DIR)/ast/stmt/compound_stmt.c \
	$(SRC_DIR)/ast/stmt/return_stmt.c \
	$(SRC_DIR)/ast/stmt/stmt.c \

# Compiler target
COMPILER_TARGET = $(BIN_DIR)/shiroc
COMPILER_SRCS = $(COMMON_SRCS) $(SRC_DIR)/main.c
COMPILER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMPILER_SRCS))

# All targets
TARGETS = $(COMPILER_TARGET)

# Default target
.PHONY: all
all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -c $< -o $@

$(COMPILER_TARGET): $(COMPILER_OBJS) | $(BIN_DIR)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -o $@ $(COMPILER_OBJS)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: compile_commands
compile_commands:
	make clean
	mkdir -p $(BUILD_DIR)
	bear --output $(BUILD_DIR)/compile_commands.json -- make all
