#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/type.h"
#include "sema/semantic_analyzer.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_expr_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
};

TEST_SETUP(ut_sema_expr_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_expr_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    semantic_context_destroy(fix->ctx);
}

// Emit error when we try to assign to a function
TEST(ut_sema_expr_fixture_t, assignment_to_function_error)
{
    // Register a function in global scope
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_VOID), ast_compound_stmt_create_empty(),
        nullptr);
    symbol_t* foo_symbol = symbol_create("foo", SYMBOL_FUNCTION, foo_fn);
    foo_symbol->type = ast_type_builtin(TYPE_VOID);
    symbol_table_insert(fix->ctx->global, foo_symbol);

    // Try to assign to the function
    ast_expr_t* error_node = ast_ref_expr_create("foo");
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(12));

    ASSERT_SEMA_ERROR(AST_NODE(expr), error_node, "not l-value");

    ast_node_destroy(expr);
    ast_node_destroy(foo_fn);
}

// Emit error when we try to assign to a binary-expression
TEST(ut_sema_expr_fixture_t, assignment_to_non_lvalue_expression_error)
{
    // Try to assign to a binary expression (5 * 3 = 30)
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_STAR, ast_int_lit_val(5), ast_int_lit_val(3));;
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(30));

    ASSERT_SEMA_ERROR(AST_NODE(expr), error_node, "not l-value");

    ast_node_destroy(expr);
}

// Emit error when we try to assign to a literal
TEST(ut_sema_expr_fixture_t, assignment_to_literal_error)
{
    // Try to assign to a literal (42 = 10)
    ast_expr_t* error_node = ast_int_lit_val(42);
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(10));

    ASSERT_SEMA_ERROR(AST_NODE(expr), error_node, "not l-value");

    ast_node_destroy(expr);
}

TEST(ut_sema_expr_fixture_t, reject_assign_to_address_of_lvalue)
{
    // var i = 5;
    // &i = 10;  // Error: not an lvalue
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(5))),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_unary_op_create(TOKEN_AMPERSAND,
            ast_ref_expr_create("i")), ast_int_lit_val(10))),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "address of l-value"));

    ast_node_destroy(AST_NODE(block));
}

// Arithmetic operations on boolean operands should produce an error
TEST(ut_sema_expr_fixture_t, arithmetic_operation_on_booleans_error)
{
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_PLUS, ast_bool_lit_create(true), ast_bool_lit_create(false));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(error_node));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot apply '+' to 'bool'"));

    ast_node_destroy(error_node);
}

// Should be able to assign to lvalue dereference
TEST(ut_sema_expr_fixture_t, assign_to_lvalue_deref)
{
    // var f = 0.0;
    // var ptr = &f;
    // *ptr += 32.5;
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("f", nullptr, ast_float_lit_create(0, ""))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr, ast_unary_op_create(TOKEN_AMPERSAND,
            ast_ref_expr_create("f")))),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS_ASSIGN,
            ast_unary_op_create(TOKEN_STAR, ast_ref_expr_create("ptr")), ast_float_lit_create(32.5, ""))),
        nullptr
        );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}
