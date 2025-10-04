# Directories
BUILD_DIR = build
UT_BUILD_DIR = $(BUILD_DIR)/tests
BIN_DIR = $(BUILD_DIR)/bin
UT_BIN_DIR = $(BIN_DIR)/tests
SRC_DIR = src
UT_SRC_DIR = $(SRC_DIR)/tests/ut

# Tools & Flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Werror=incompatible-pointer-types -Wsign-conversion -Wshadow  \
		 -std=c23 -I$(SRC_DIR)
DEBUGFLAGS = -g -O0
LD = ld

# Common source files
COMMON_SRCS = \
    $(SRC_DIR)/lexer.c \
    $(SRC_DIR)/parser.c \
    $(SRC_DIR)/ast/node.c \
	$(SRC_DIR)/ast/printer.c \
	$(SRC_DIR)/ast/root.c \
	$(SRC_DIR)/ast/visitor.c \
	$(SRC_DIR)/ast/def/def.c \
    $(SRC_DIR)/ast/def/fn_def.c \
	$(SRC_DIR)/ast/expr/expr.c \
	$(SRC_DIR)/ast/expr/int_lit.c \
	$(SRC_DIR)/ast/stmt/compound_stmt.c \
	$(SRC_DIR)/ast/stmt/return_stmt.c \
	$(SRC_DIR)/ast/stmt/stmt.c \
	$(SRC_DIR)/common/containers/string.c \
	$(SRC_DIR)/common/containers/vec.c
COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

# Unit-tests source files
UT_SRCS = \
	$(UT_SRC_DIR)/ut_parser.c

# Compiler target
COMPILER_TARGET = $(BIN_DIR)/shiroc
COMPILER_SRCS = $(COMMON_SRCS) $(SRC_DIR)/main.c
COMPILER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMPILER_SRCS))

# Unit-tests target
UT_TARGETS = $(patsubst $(UT_SRC_DIR)/%.c,$(UT_BIN_DIR)/%.test,$(UT_SRCS))
UT_OBJS = $(patsubst $(UT_SRC_DIR)/%.c,$(UT_BUILD_DIR)/%.o,$(UT_SRCS))
TEST_RUNNER_OBJ = $(BUILD_DIR)/test-runner.o
TEST_RUNNER_SRC = $(SRC_DIR)/common/test-runner/test_runner.c
TEST_RUNNER_INCLUDE = $(SRC_DIR)/common/test-runner

# All targets
TARGETS = $(COMPILER_TARGET) $(UT_TARGETS)

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

$(UT_BIN_DIR):
	mkdir -p $@

$(TEST_RUNNER_OBJ): $(TEST_RUNNER_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -c $< -o $@

$(UT_BIN_DIR)/%.test: $(UT_SRC_DIR)/%.c $(TEST_RUNNER_OBJ) $(COMMON_OBJS) | $(UT_BIN_DIR)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -I$(TEST_RUNNER_INCLUDE) $^ -o $@

tests: $(UT_TARGETS)
	@for test in $(UT_TARGETS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: compile_commands
compile_commands:
	make clean
	mkdir -p $(BUILD_DIR)
	bear --output $(BUILD_DIR)/compile_commands.json -- make all
