# Directories
BUILD_DIR = build
UT_BUILD_DIR = $(BUILD_DIR)/tests
BIN_DIR = $(BUILD_DIR)/bin
UT_BIN_DIR = $(BIN_DIR)/tests
SRC_DIR = src
UT_SRC_DIR = $(SRC_DIR)/tests/ut

# Tools & Flags
CC = gcc

# LLVM Configuration - auto-detect version
LLVM_CONFIG := $(shell which llvm-config 2>/dev/null || \
                       which llvm-config-19 2>/dev/null || \
                       which llvm-config-18 2>/dev/null || \
                       which llvm-config-17 2>/dev/null || \
                       echo "")
ifeq ($(LLVM_CONFIG),)
    $(error "No llvm-config found. Please install LLVM development package")
endif
LLVM_CFLAGS := $(shell $(LLVM_CONFIG) --cflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --libs core)

CFLAGS = -Wall -Wextra -Werror=incompatible-pointer-types -Wsign-conversion -Wshadow  \
		 -std=c23 -I$(SRC_DIR) $(LLVM_CFLAGS)
LDFLAGS = $(LLVM_LDFLAGS) $(LLVM_LIBS)
DEBUGFLAGS = -g -O0
LD = ld
FUZZ_CC = clang
FUZZ_CFLAGS = -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -g -O1 -I$(SRC_DIR) -std=c23
FUZZ_LFLAGS = -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined

# Coverage tools & flags
COV_CC = clang
COV_CFLAGS = -fprofile-instr-generate -fcoverage-mapping -g -O0 -I$(SRC_DIR) \
			 -Wall -Wextra -Wsign-conversion -Wshadow -std=c23
COV_LDFLAGS = -fprofile-instr-generate -fcoverage-mapping
COV_BUILD_DIR = $(BUILD_DIR)/coverage
COV_BIN_DIR = $(BIN_DIR)/coverage
COV_REPORT_DIR = $(BUILD_DIR)/coverage_report
LLVM_PROFDATA = llvm-profdata
LLVM_COV = llvm-cov

# Common source files
COMMON_SRCS = \
	$(SRC_DIR)/compiler_error.c \
	$(SRC_DIR)/ast/node.c \
	$(SRC_DIR)/ast/root.c \
	$(SRC_DIR)/ast/transformer.c \
	$(SRC_DIR)/ast/type.c \
	$(SRC_DIR)/ast/visitor.c \
	$(SRC_DIR)/ast/decl/member_decl.c \
	$(SRC_DIR)/ast/decl/param_decl.c \
	$(SRC_DIR)/ast/decl/var_decl.c \
	$(SRC_DIR)/ast/def/def.c \
	$(SRC_DIR)/ast/def/class_def.c \
	$(SRC_DIR)/ast/def/fn_def.c \
	$(SRC_DIR)/ast/def/import_def.c \
	$(SRC_DIR)/ast/def/method_def.c \
	$(SRC_DIR)/ast/expr/access_expr.c \
	$(SRC_DIR)/ast/expr/array_lit.c \
	$(SRC_DIR)/ast/expr/array_slice.c \
	$(SRC_DIR)/ast/expr/array_subscript.c \
	$(SRC_DIR)/ast/expr/bin_op.c \
	$(SRC_DIR)/ast/expr/bool_lit.c \
	$(SRC_DIR)/ast/expr/call_expr.c \
	$(SRC_DIR)/ast/expr/cast_expr.c \
	$(SRC_DIR)/ast/expr/coercion_expr.c \
	$(SRC_DIR)/ast/expr/construct_expr.c \
	$(SRC_DIR)/ast/expr/expr.c \
	$(SRC_DIR)/ast/expr/float_lit.c \
	$(SRC_DIR)/ast/expr/int_lit.c \
	$(SRC_DIR)/ast/expr/member_access.c \
	$(SRC_DIR)/ast/expr/member_init.c \
	$(SRC_DIR)/ast/expr/method_call.c \
	$(SRC_DIR)/ast/expr/null_lit.c \
	$(SRC_DIR)/ast/expr/paren_expr.c \
	$(SRC_DIR)/ast/expr/ref_expr.c \
	$(SRC_DIR)/ast/expr/self_expr.c \
	$(SRC_DIR)/ast/expr/str_lit.c \
	$(SRC_DIR)/ast/expr/unary_op.c \
	$(SRC_DIR)/ast/expr/uninit_lit.c \
	$(SRC_DIR)/ast/stmt/break_stmt.c \
	$(SRC_DIR)/ast/stmt/compound_stmt.c \
	$(SRC_DIR)/ast/stmt/continue_stmt.c \
	$(SRC_DIR)/ast/stmt/decl_stmt.c \
	$(SRC_DIR)/ast/stmt/expr_stmt.c \
	$(SRC_DIR)/ast/stmt/for_stmt.c \
	$(SRC_DIR)/ast/stmt/if_stmt.c \
	$(SRC_DIR)/ast/stmt/inc_dec_stmt.c \
	$(SRC_DIR)/ast/stmt/return_stmt.c \
	$(SRC_DIR)/ast/stmt/stmt.c \
	$(SRC_DIR)/ast/stmt/while_stmt.c \
	$(SRC_DIR)/ast/util/cloner.c \
	$(SRC_DIR)/ast/util/presenter.c \
	$(SRC_DIR)/ast/util/printer.c \
	$(SRC_DIR)/common/toml_parser.c \
	$(SRC_DIR)/common/containers/hash_table.c \
	$(SRC_DIR)/common/containers/string.c \
	$(SRC_DIR)/common/containers/vec.c \
	$(SRC_DIR)/common/util/path.c \
	$(SRC_DIR)/parser/lexer.c \
    $(SRC_DIR)/parser/parser.c \
	$(SRC_DIR)/sema/access_transformer.c \
	$(SRC_DIR)/sema/decl_collector.c \
	$(SRC_DIR)/sema/expr_evaluator.c \
	$(SRC_DIR)/sema/init_tracker.c \
	$(SRC_DIR)/sema/semantic_analyzer.c \
	$(SRC_DIR)/sema/semantic_context.c \
	$(SRC_DIR)/sema/symbol_table.c \
	$(SRC_DIR)/sema/symbol.c \
	$(SRC_DIR)/sema/type_resolver.c
COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

# Unit-tests source files
UT_SRCS = \
	$(UT_SRC_DIR)/ut_decl_collector.c \
	$(UT_SRC_DIR)/ut_hash_table.c \
	$(UT_SRC_DIR)/test_toml_parser.c \
	$(UT_SRC_DIR)/parser/test_parser_arrays.c \
	$(UT_SRC_DIR)/parser/test_parser_classes.c \
	$(UT_SRC_DIR)/parser/test_parser_errors.c \
	$(UT_SRC_DIR)/parser/test_parser_expressions.c \
	$(UT_SRC_DIR)/parser/test_parser_functions.c \
	$(UT_SRC_DIR)/parser/test_parser_literals.c \
	$(UT_SRC_DIR)/parser/test_parser_misc.c \
	$(UT_SRC_DIR)/parser/test_parser_pointers.c \
	$(UT_SRC_DIR)/parser/test_parser_statements.c \
	$(UT_SRC_DIR)/sema/test_sema_access.c \
	$(UT_SRC_DIR)/sema/test_sema_arrays.c \
	$(UT_SRC_DIR)/sema/test_sema_casts.c \
	$(UT_SRC_DIR)/sema/test_sema_classes.c \
	$(UT_SRC_DIR)/sema/test_sema_control_flow.c \
	$(UT_SRC_DIR)/sema/test_sema_expressions.c \
	$(UT_SRC_DIR)/sema/test_sema_functions.c \
	$(UT_SRC_DIR)/sema/test_sema_types.c \
	$(UT_SRC_DIR)/sema/test_sema_variables.c

# Compiler target
COMPILER_TARGET = $(BIN_DIR)/shiro
COMPILER_SRCS = $(COMMON_SRCS) $(SRC_DIR)/main.c $(SRC_DIR)/codegen/llvm/llvm_codegen.c \
	$(SRC_DIR)/codegen/llvm/llvm_type_utils.c $(SRC_DIR)/builder/builder.c \
	$(SRC_DIR)/builder/module.c
COMPILER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMPILER_SRCS))

# Unit-tests target
UT_TARGETS = $(patsubst $(UT_SRC_DIR)/%.c,$(UT_BIN_DIR)/%.test,$(UT_SRCS))
UT_OBJS = $(patsubst $(UT_SRC_DIR)/%.c,$(UT_BUILD_DIR)/%.o,$(UT_SRCS))
TEST_RUNNER_OBJ = $(BUILD_DIR)/test-runner.o
TEST_RUNNER_SRC = $(SRC_DIR)/common/test-runner/test_runner.c
TEST_RUNNER_INCLUDE = $(SRC_DIR)/common/test-runner

# Coverage-instrumented unit tests
COV_UT_TARGETS = $(patsubst $(UT_SRC_DIR)/%.c,$(COV_BIN_DIR)/%.test,$(UT_SRCS))
COV_COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c,$(COV_BUILD_DIR)/%.o,$(COMMON_SRCS))
COV_TEST_RUNNER_OBJ = $(COV_BUILD_DIR)/test-runner.o

# Fuzzer targets
FUZZER_TARGET = $(BIN_DIR)/shiro_fuzzer
FUZZER_SRC = $(SRC_DIR)/common/fuzzer/fuzzer_harness.c
FUZZER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/fuzzer/%.o,$(COMMON_SRCS))
FUZZER_HARNESS_OBJ = $(BUILD_DIR)/fuzzer/common/fuzzer/fuzzer_harness.o
FUZZ_CORPUS_DIR = $(BUILD_DIR)/fuzz_corpus
FUZZ_CORPUS_MIN_DIR = $(BUILD_DIR)/fuzz_corpus_min

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
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -o $@ $(COMPILER_OBJS) $(LDFLAGS)
	cp $(SRC_DIR)/runtime/builtins.c $(BIN_DIR)/builtins.c

$(UT_BIN_DIR):
	mkdir -p $@

$(TEST_RUNNER_OBJ): $(TEST_RUNNER_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -c $< -o $@

$(UT_BIN_DIR)/%.test: $(UT_SRC_DIR)/%.c $(TEST_RUNNER_OBJ) $(COMMON_OBJS) | $(UT_BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(DEBUGFLAGS) $(CFLAGS) -I$(TEST_RUNNER_INCLUDE) $^ -o $@

# Coverage-instrumented build rules
$(COV_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(COV_CC) $(COV_CFLAGS) -c $< -o $@

$(COV_TEST_RUNNER_OBJ): $(TEST_RUNNER_SRC)
	@mkdir -p $(dir $@)
	$(COV_CC) $(COV_CFLAGS) -c $< -o $@

$(COV_BIN_DIR):
	mkdir -p $@

$(COV_BIN_DIR)/%.test: $(UT_SRC_DIR)/%.c $(COV_TEST_RUNNER_OBJ) $(COV_COMMON_OBJS) | $(COV_BIN_DIR)
	@mkdir -p $(dir $@)
	$(COV_CC) $(COV_CFLAGS) $(COV_LDFLAGS) -I$(TEST_RUNNER_INCLUDE) $^ -o $@

$(BUILD_DIR)/fuzzer/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -c $< -o $@

$(FUZZER_TARGET): $(FUZZER_OBJS) $(FUZZER_HARNESS_OBJ) | $(BIN_DIR)
	$(FUZZ_CC) $(FUZZ_LFLAGS) -o $@ $^

test-ut: $(UT_TARGETS)
	@for test in $(UT_TARGETS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

test-st: $(BIN_DIR)
	python3 scripts/systemtester/tester.py -r -c ./build/bin/shiro src/tests/st/

.PHONY: tests
tests: test-ut test-st

valgrind-ut: $(UT_TARGETS)
	@for test in $(UT_TARGETS); do \
		echo ""; \
		echo "Running $$test with valgrind..."; \
		valgrind --leak-check=full \
		     --num-callers=100 \
	         --show-leak-kinds=all \
	         --errors-for-leak-kinds=all \
	         --error-exitcode=1 \
	         --track-origins=yes \
	         --read-var-info=yes \
	         --expensive-definedness-checks=yes \
			 --exit-on-first-error=yes \
			 --suppressions=src/tests/valgrind.supp \
	         ./$$test || exit 1; \
	done

valgrind-st:  $(BIN_DIR)
	python3 ./scripts/systemtester/tester.py --valgrind -r -c ./build/bin/shiro src/tests/st/

.PHONY: valgrind-tests
valgrind-tests: valgrind-ut valgrind-st

.PHONY: fuzzer
fuzzer: $(FUZZER_TARGET)

.PHONY: fuzzing
fuzzing: $(FUZZER_TARGET)
	mkdir -p $(FUZZ_CORPUS_DIR)
	mkdir -p $(BUILD_DIR)/fuzz_artifacts
	find src/tests/st -name "*.shiro" -exec cp {} $(FUZZ_CORPUS_DIR)/ \;
	$(FUZZER_TARGET) $(FUZZ_CORPUS_DIR)/ -artifact_prefix=$(BUILD_DIR)/fuzz_artifacts/ -max_len=50000 -timeout=5

.PHONY: fuzzing-minimize
fuzzing-minimize: $(FUZZER_TARGET)
	mkdir -p $(FUZZ_CORPUS_MIN_DIR)
	$(FUZZER_TARGET) -merge=1 $(FUZZ_CORPUS_MIN_DIR)/ $(FUZZ_CORPUS_DIR)/

.PHONY: coverage
coverage: $(COV_UT_TARGETS)
	@echo "Running coverage-instrumented unit tests..."
	@rm -rf $(COV_REPORT_DIR)
	@mkdir -p $(COV_REPORT_DIR)
	@rm -f $(BUILD_DIR)/*.profraw $(BUILD_DIR)/coverage.profdata
	@for test in $(COV_UT_TARGETS); do \
		echo ""; \
		echo "Running $$test..."; \
		LLVM_PROFILE_FILE=$(BUILD_DIR)/$$(basename $$test).profraw ./$$test || exit 1; \
	done
	@echo ""
	@echo "Merging coverage data..."
	$(LLVM_PROFDATA) merge -sparse $(BUILD_DIR)/*.profraw -o $(BUILD_DIR)/coverage.profdata
	@echo "Generating coverage report..."
	$(LLVM_COV) show $(COV_UT_TARGETS) \
		-instr-profile=$(BUILD_DIR)/coverage.profdata \
		-format=html \
		-output-dir=$(COV_REPORT_DIR) \
		-show-line-counts-or-regions \
		-show-instantiations=false \
		$(COMMON_SRCS)
	@echo ""
	@echo "Coverage report generated in $(COV_REPORT_DIR)/index.html"
	@echo "Summary:"
	@$(LLVM_COV) report $(COV_UT_TARGETS) \
		-instr-profile=$(BUILD_DIR)/coverage.profdata \
		$(COMMON_SRCS)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: compile_commands
compile_commands:
	make clean
	mkdir -p $(BUILD_DIR)
	bear --output $(BUILD_DIR)/compile_commands.json -- make all
