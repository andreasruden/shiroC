#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/null_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"
#include <float.h>
#include <stdint.h>

TEST_FIXTURE(ut_sema_type_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_type_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_type_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
}

// Assignment with incompatible types
TEST(ut_sema_type_fixture_t, assignment_with_mismatched_types)
{
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_val(true));

    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_BOOL), nullptr)),
        // Error: assign i32 to bool
        ast_expr_stmt_create(error_node),
        nullptr
    ), nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "cannot coerce type");

    ast_node_destroy(foo_fn);
}

// Test that various invalid combinations of integer literals and suffixes get rejected
TEST(ut_sema_type_fixture_t, reject_invalid_int_literals)
{
    ast_expr_t* error_nodes[] =
    {
        ast_int_lit_create(false, 256, "i8"),        // 256i8 - too large for i8
        ast_int_lit_create(false, 128, "i8"),        // 128i8 - too large for i8
        ast_int_lit_create(true, 129, "i8"),         // -129i8 - too small for i8
        ast_int_lit_create(false, 65536, "i16"),     // 65536i16 - too large for i16
        ast_int_lit_create(false, 32768, "i16"),     // 32768i16 - too large for i16
        ast_int_lit_create(true, 32769, "i16"),      // -32769i16 - too small for i16
        ast_int_lit_create(false, 2147483648ULL, "i32"),  // 2^31 - too large for i32
        ast_int_lit_create(true, 2147483649ULL, "i32"),   // -2147483649i32 - too small for i32
        ast_int_lit_create(false, 9223372036854775808ULL, "i64"),  // 2^63 - too large for i64
        ast_int_lit_create(true, 9223372036854775809ULL, "i64"),   // -(2^63+1) - too small for i64
        ast_int_lit_create(true, 1, "u8"),           // -1u8 - negative with unsigned
        ast_int_lit_create(true, 100, "u16"),        // -100u16 - negative with unsigned
        ast_int_lit_create(true, 1000, "u32"),       // -1000u32 - negative with unsigned
        ast_int_lit_create(true, 50000, "u64"),      // -50000u64 - negative with unsigned
        ast_int_lit_create(false, 256, "u8"),        // 256u8 - too large for u8
        ast_int_lit_create(false, 65536, "u16"),     // 65536u16 - too large for u16
        ast_int_lit_create(false, 4294967296ULL, "u32"),  // 2^32 - too large for u32
        // Overflow of u64 is handled in the parser due to restrictions of repr we're using.
        ast_int_lit_create(false, 100, "i128"),      // invalid suffix
        ast_int_lit_create(false, 100, "u128"),      // invalid suffix
        ast_int_lit_create(false, 100, "int"),       // invalid suffix
        ast_int_lit_create(false, 100, "f32"),       // invalid suffix (float)
    };

    size_t num_tests = sizeof(error_nodes) / sizeof(error_nodes[0]);

    for (size_t i = 0; i < num_tests; ++i)
    {
        bool res = semantic_analyzer_run(fix->sema, AST_NODE(error_nodes[i]));
        ASSERT_FALSE(res);
        ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
        compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);

        // Check that error message is appropriate
        bool has_overflow_error = strstr(error->description, "does not fit") != nullptr;
        bool has_negative_error = strstr(error->description, "negative") != nullptr;
        bool has_invalid_suffix = strstr(error->description, "invalid") != nullptr;

        ASSERT_TRUE(has_overflow_error || has_negative_error || has_invalid_suffix);

        // Reset error state:
        vec_deinit(&fix->ctx->error_nodes);
        fix->ctx->error_nodes = VEC_INIT(nullptr);
    }

    for (size_t i = 0; i < num_tests; ++i)
        ast_node_destroy(AST_NODE(error_nodes[i]));
}

// Test that various valid integer literals are accepted and have correct values
TEST(ut_sema_type_fixture_t, accept_valid_int_literals)
{
    struct test_case {
        ast_expr_t* node;
        bool is_signed;
        union {
            int64_t as_signed;
            uint64_t as_unsigned;
        } expected_value;
    };

    struct test_case test_cases[] =
    {
        // i8 cases
        {ast_int_lit_create(false, 0, "i8"), true, .expected_value.as_signed = 0},
        {ast_int_lit_create(false, 127, "i8"), true, .expected_value.as_signed = 127},
        {ast_int_lit_create(true, 128, "i8"), true, .expected_value.as_signed = -128},
        {ast_int_lit_create(true, 1, "i8"), true, .expected_value.as_signed = -1},

        // i16 cases
        {ast_int_lit_create(false, 32767, "i16"), true, .expected_value.as_signed = 32767},
        {ast_int_lit_create(true, 32768, "i16"), true, .expected_value.as_signed = -32768},

        // i32 cases
        {ast_int_lit_create(false, 2147483647, "i32"), true, .expected_value.as_signed = 2147483647},
        {ast_int_lit_create(true, 2147483648ULL, "i32"), true, .expected_value.as_signed = -2147483648LL},
        {ast_int_lit_create(false, 42, ""), true, .expected_value.as_signed = 42},  // default i32
        {ast_int_lit_create(true, 42, ""), true, .expected_value.as_signed = -42},  // default i32

        // i64 cases
        {ast_int_lit_create(false, 9223372036854775807ULL, "i64"), true, .expected_value.as_signed = 9223372036854775807LL},
        {ast_int_lit_create(true, 9223372036854775808ULL, "i64"), true, .expected_value.as_signed = INT64_MIN},

        // u8 cases
        {ast_int_lit_create(false, 0, "u8"), false, .expected_value.as_unsigned = 0},
        {ast_int_lit_create(false, 255, "u8"), false, .expected_value.as_unsigned = 255},

        // u16 cases
        {ast_int_lit_create(false, 65535, "u16"), false, .expected_value.as_unsigned = 65535},

        // u32 cases
        {ast_int_lit_create(false, 4294967295ULL, "u32"), false, .expected_value.as_unsigned = 4294967295ULL},

        // u64 cases
        {ast_int_lit_create(false, 18446744073709551615ULL, "u64"), false, .expected_value.as_unsigned = 18446744073709551615ULL},
        {ast_int_lit_create(false, 0, "u64"), false, .expected_value.as_unsigned = 0},
    };

    size_t num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_tests; ++i)
    {
        bool res = semantic_analyzer_run(fix->sema, AST_NODE(test_cases[i].node));
        ASSERT_TRUE(res);
        ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

        ast_int_lit_t* lit = (ast_int_lit_t*)test_cases[i].node;

        // Verify type is set
        ASSERT_NEQ(nullptr, lit->base.type);

        // Verify value matches expected
        if (test_cases[i].is_signed)
        {
            ASSERT_TRUE(ast_type_is_signed(lit->base.type));
            ASSERT_EQ(test_cases[i].expected_value.as_signed, lit->value.as_signed);
        }
        else
        {
            ASSERT_FALSE(ast_type_is_signed(lit->base.type));
            ASSERT_EQ(test_cases[i].expected_value.as_unsigned, lit->value.as_unsigned);
        }
    }

    for (size_t i = 0; i < num_tests; ++i)
        ast_node_destroy(AST_NODE(test_cases[i].node));
}

// Test that float literals that are too large for f32 are rejected
TEST(ut_sema_type_fixture_t, reject_float_literal_overflow_f32)
{
    ast_expr_t* error_nodes[] =
    {
        // Values larger than FLT_MAX (~3.4e38)
        ast_float_lit_create(1e39, "f32"),
        ast_float_lit_create(3.5e38, "f32"),
        ast_float_lit_create(-1e39, "f32"),
        ast_float_lit_create(999999999999999999999999999999999999999.0, "f32"),

        // Invalid suffixes
        ast_float_lit_create(3.14, "f16"),
        ast_float_lit_create(2.71, "float"),
        ast_float_lit_create(1.0, "double"),
        ast_float_lit_create(1.5, "i32"),
    };

    size_t num_tests = sizeof(error_nodes) / sizeof(error_nodes[0]);

    for (size_t i = 0; i < num_tests; i++)
    {
        bool res = semantic_analyzer_run(fix->sema, AST_NODE(error_nodes[i]));
        ASSERT_FALSE(res);
        ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
        compiler_error_t* error = vec_get(AST_NODE(vec_get(&fix->ctx->error_nodes, 0))->errors, 0);

        // Check that error message mentions overflow or invalid suffix
        bool has_overflow = strstr(error->description, "too large") != nullptr;
        bool has_invalid_suffix = strstr(error->description, "invalid") != nullptr;

        ASSERT_TRUE(has_overflow || has_invalid_suffix);

        // Reset error state:
        vec_deinit(&fix->ctx->error_nodes);
        fix->ctx->error_nodes = VEC_INIT(nullptr);
    }

    for (size_t i = 0; i < num_tests; i++)
        ast_node_destroy(AST_NODE(error_nodes[i]));
}

// Test that valid float literals are accepted and have correct values
TEST(ut_sema_type_fixture_t, accept_valid_float_literals)
{
    struct test_case {
        ast_expr_t* node;
        bool is_f32;
        union {
            float as_f32;
            double as_f64;
        } expected_value;
    };

    struct test_case test_cases[] =
    {
        // f32 cases
        {ast_float_lit_create(3.14, "f32"), true, .expected_value.as_f32 = 3.14f},
        {ast_float_lit_create(0.0, "f32"), true, .expected_value.as_f32 = 0.0f},
        {ast_float_lit_create(-2.5, "f32"), true, .expected_value.as_f32 = -2.5f},
        {ast_float_lit_create(1e10, "f32"), true, .expected_value.as_f32 = 1e10f},
        {ast_float_lit_create(1.5e-5, "f32"), true, .expected_value.as_f32 = 1.5e-5f},
        {ast_float_lit_create(3.4e38, "f32"), true, .expected_value.as_f32 = 3.4e38f},  // Near FLT_MAX

        // f64 cases
        {ast_float_lit_create(3.14159265358979, "f64"), false, .expected_value.as_f64 = 3.14159265358979},
        {ast_float_lit_create(2.718281828, "f64"), false, .expected_value.as_f64 = 2.718281828},
        {ast_float_lit_create(1e100, "f64"), false, .expected_value.as_f64 = 1e100},
        {ast_float_lit_create(-9.8, "f64"), false, .expected_value.as_f64 = -9.8},

        // Default f64 (no suffix)
        {ast_float_lit_create(42.0, ""), false, .expected_value.as_f64 = 42.0},
        {ast_float_lit_create(1.23, ""), false, .expected_value.as_f64 = 1.23},
    };

    size_t num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_tests; i++)
    {
        bool res = semantic_analyzer_run(fix->sema, AST_NODE(test_cases[i].node));
        ASSERT_TRUE(res);
        ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

        ast_float_lit_t* lit = (ast_float_lit_t*)test_cases[i].node;

        // Verify type is set
        ASSERT_NEQ(nullptr, lit->base.type);

        if (test_cases[i].is_f32)
        {
            ASSERT_EQ(lit->base.type, ast_type_builtin(TYPE_F32));
            ASSERT_EQ(test_cases[i].expected_value.as_f32, (float)lit->value);
        }
        else
        {
            ASSERT_EQ(lit->base.type, ast_type_builtin(TYPE_F64));
            ASSERT_EQ(test_cases[i].expected_value.as_f64, lit->value);
        }
    }

    for (size_t i = 0; i < num_tests; i++)
        ast_node_destroy(AST_NODE(test_cases[i].node));
}

TEST(ut_sema_type_fixture_t, accept_assign_address_to_pointer)
{
    // var i = 10;
    // var ptr = &i;
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(10))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr, ast_unary_op_create(TOKEN_AMPERSAND,
            ast_ref_expr_create("i")))),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(AST_NODE(block));
}

TEST(ut_sema_type_fixture_t, reject_assign_value_to_pointer)
{
    // var i = 10;
    // var ptr: i32*;
    // ptr = i;  // Error: can't assign i32 to i32*
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("ptr"), ast_ref_expr_create("i"));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(10))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_I32)), nullptr)),
        ast_expr_stmt_create(error_node),
        nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot coerce type");
    /*bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, ""));*/

    ast_node_destroy(AST_NODE(block));
}

TEST(ut_sema_type_fixture_t, accept_dereference_pointer)
{
    // var value = 10;
    // var ptr = &value;
    // value = *ptr;
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("value", nullptr, ast_int_lit_val(10))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr, ast_unary_op_create(TOKEN_AMPERSAND,
            ast_ref_expr_create("value")))),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("value"),
            ast_unary_op_create(TOKEN_STAR, ast_ref_expr_create("ptr")))),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(AST_NODE(block));
}

TEST(ut_sema_type_fixture_t, reject_null_without_type_annotation)
{
    // var ptr = null;  // Error: cannot infer type from null
    ast_stmt_t* stmt = ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr, ast_null_lit_create()));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(stmt));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot infer type from 'null'"));

    ast_node_destroy(AST_NODE(stmt));
}

TEST(ut_sema_type_fixture_t, accept_null_with_type_annotation)
{
    // var ptr: i32* = null;
    ast_stmt_t* stmt = ast_decl_stmt_create(
        ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_I32)), ast_null_lit_create()));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(stmt));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    symbol_t* symbol = symbol_table_lookup(fix->ctx->current, "ptr");
    ASSERT_NEQ(nullptr, symbol);
    ASSERT_EQ(symbol->type, ast_type_pointer(ast_type_builtin(TYPE_I32)));

    ast_node_destroy(AST_NODE(stmt));
}

TEST(ut_sema_type_fixture_t, reject_null_assigned_to_non_pointer_type)
{
    // var ptr: i32 = null;  // Error: annotated type is not pointer
    ast_stmt_t* stmt = ast_decl_stmt_create(
        ast_var_decl_create("ptr", ast_type_builtin(TYPE_I32), ast_null_lit_create()));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(stmt));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot assign 'null' to non-pointer type"));

    ast_node_destroy(AST_NODE(stmt));
}

TEST(ut_sema_type_fixture_t, accept_null_comparison_in_function)
{
    // foo(ptr: bool*) { if (ptr == null) {} }
    ast_def_t* func = ast_fn_def_create_va(
        "foo", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_va(
            ast_if_stmt_create(ast_bin_op_create(TOKEN_EQ, ast_ref_expr_create("ptr"), ast_null_lit_create()),
                ast_compound_stmt_create_empty(), nullptr), nullptr),
        ast_param_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_BOOL))),
        nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(func));

    ast_node_destroy(AST_NODE(func));
}

// Verify ptr is assumed to be initialized with null
TEST(ut_sema_type_fixture_t, initialize_ptr_with_null)
{
    // var ptr: f32* = null;
    // if (ptr == null) {}
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_F32)),
            ast_null_lit_create())),
        ast_if_stmt_create(ast_bin_op_create(TOKEN_EQ, ast_ref_expr_create("ptr"), ast_null_lit_create()),
            ast_compound_stmt_create_empty(), nullptr),
        nullptr
        );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}

TEST(ut_sema_type_fixture_t, implicit_conversion_array_to_view)
{
    // fn inspect(data: view[i32]) {}
    // fn test_fn() {
    //     var arr = [5, 6, 7];
    //     inspect(arr);  // OK: implicit conversion from [i32, 3] to view[i32]
    // }

    ast_def_t* inspect_fn = ast_fn_def_create_va(
        "inspect", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(),
        ast_param_decl_create("data", ast_type_view(ast_type_builtin(TYPE_I32))),
        nullptr
    );

    ast_expr_t* call_expr = ast_call_expr_create_va(
        ast_ref_expr_create("inspect"),
        ast_ref_expr_create("arr"),
        nullptr
    );

    ast_def_t* test_fn = ast_fn_def_create_va(
        "test_fn", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("arr",
                nullptr,
                ast_array_lit_create_va(ast_int_lit_val(5), ast_int_lit_val(6), ast_int_lit_val(7), nullptr))),
            ast_expr_stmt_create(call_expr),
            nullptr
        ),
        nullptr
    );

    ast_root_t* root = ast_root_create_va(inspect_fn, test_fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify that a coercion node was inserted
    ast_node_t* first_arg = vec_get(&((ast_call_expr_t*)call_expr)->arguments, 0);
    ASSERT_EQ(AST_EXPR_COERCION, first_arg->kind);

    ast_node_destroy(root);
}
