#include "ast/decl/var_decl.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/for_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/inc_dec_stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/type.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_cf_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
};

TEST_SETUP(ut_sema_cf_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_cf_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    semantic_context_destroy(fix->ctx);
}

// If statement with non-boolean condition
TEST(ut_sema_cf_fixture_t, if_condition_must_be_boolean_expr)
{
    ast_expr_t* error_node = ast_int_lit_val(42);

    ast_stmt_t* if_stmt = ast_if_stmt_create(
        error_node,  // Should be boolean
        ast_compound_stmt_create_empty(),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(if_stmt), error_node, "must be bool");

    ast_node_destroy(if_stmt);
}

// While statement with non-boolean condition
TEST(ut_sema_cf_fixture_t, while_condition_must_be_boolean_expr)
{
    ast_expr_t* error_node = ast_int_lit_val(42);

    ast_stmt_t* while_stmt = ast_while_stmt_create(
        error_node,  // Should be boolean
        ast_compound_stmt_create_empty());

    ASSERT_SEMA_ERROR(AST_NODE(while_stmt), error_node, "must be bool");

    ast_node_destroy(while_stmt);
}

// Comparison is valid in if-condition
TEST(ut_sema_cf_fixture_t, comparison_in_if_condition)
{
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(3))),
        ast_if_stmt_create(ast_bin_op_create(TOKEN_GT, ast_ref_expr_create("i"), ast_int_lit_val(5)),
            ast_compound_stmt_create_empty(), nullptr),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);

    ast_node_destroy(block);
}

// For statement with non-boolean condition
TEST(ut_sema_cf_fixture_t, for_condition_must_be_boolean_expr)
{
    ast_expr_t* error_node = ast_int_lit_val(42);

    ast_stmt_t* for_stmt = ast_for_stmt_create(
        nullptr,
        error_node,  // Should be boolean
        nullptr,
        ast_compound_stmt_create_empty());

    ASSERT_SEMA_ERROR(AST_NODE(for_stmt), error_node, "must be bool");

    ast_node_destroy(for_stmt);
}

// Comparison is valid in for-condition
TEST(ut_sema_cf_fixture_t, for_comparison_in_condition)
{
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(0))),
        ast_for_stmt_create(
            nullptr,
            ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(10)),
            ast_inc_dec_stmt_create(ast_ref_expr_create("i"), true),
            ast_compound_stmt_create_empty()),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);

    ast_node_destroy(block);
}

// For loop init declares variable in scope
TEST(ut_sema_cf_fixture_t, for_init_declares_variable_in_scope)
{
    ast_stmt_t* for_stmt = ast_for_stmt_create(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(0))),
        ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(10)),
        nullptr,
        ast_compound_stmt_create_va(
            ast_expr_stmt_create(ast_ref_expr_create("i")),
            nullptr));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(for_stmt));
    ASSERT_TRUE(res);

    ast_node_destroy(for_stmt);
}

// For loop post can reference init variable
TEST(ut_sema_cf_fixture_t, for_post_can_reference_init_variable)
{
    ast_stmt_t* for_stmt = ast_for_stmt_create(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(0))),
        ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(10)),
        ast_inc_dec_stmt_create(ast_ref_expr_create("i"), true),
        ast_compound_stmt_create_empty());

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(for_stmt));
    ASSERT_TRUE(res);

    ast_node_destroy(for_stmt);
}

// For loop with all parts nullable is valid
TEST(ut_sema_cf_fixture_t, for_all_parts_nullable_is_valid)
{
    ast_stmt_t* for_stmt = ast_for_stmt_create(
        nullptr,
        nullptr,
        nullptr,
        ast_compound_stmt_create_empty());

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(for_stmt));
    ASSERT_TRUE(res);

    ast_node_destroy(for_stmt);
}
