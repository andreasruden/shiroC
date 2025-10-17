#include "ast/decl/decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "sema/semantic_analyzer.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_fn_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
};

TEST_SETUP(ut_sema_fn_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_fn_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    semantic_context_destroy(fix->ctx);
}

// Function parameters should not emit uninitialized errors
TEST(ut_sema_fn_fixture_t, assume_function_parameter_is_initialized)
{
    ast_decl_t* param = ast_param_decl_create("param", ast_type_builtin(TYPE_I32));
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
            ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("param"), ast_int_lit_val(42))),
            nullptr),
    param, nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}

// Calling something that isn't a function should produce an error
TEST(ut_sema_fn_fixture_t, call_expr_must_be_ref_function_symbol)
{
    ast_expr_t* error_node = ast_ref_expr_create("not_a_function");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("not_a_function", ast_type_builtin(TYPE_I32),
            ast_int_lit_val(5))),
        ast_expr_stmt_create(ast_call_expr_create_va(error_node, nullptr)),
        nullptr
    ), nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(foo_fn), error_node, "not callable");

    ast_node_destroy(foo_fn);
}

// Calling a function with wrong number of arguments
TEST(ut_sema_fn_fixture_t, call_expr_arg_count_mismatch_error)
{
    ast_decl_t* param_x = ast_param_decl_create("x", ast_type_builtin(TYPE_I32));
    ast_decl_t* param_y = ast_param_decl_create("y", ast_type_builtin(TYPE_I32));
    ast_def_t* bar_fn = ast_fn_def_create_va("bar", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(),
        param_x, param_y, nullptr
    );

    // Register bar function in global symbol table
    symbol_t* bar_symbol = symbol_create("bar", SYMBOL_FUNCTION, bar_fn);
    bar_symbol->type = ast_type_builtin(TYPE_VOID);
    vec_push(&bar_symbol->data.function.parameters, param_x);
    vec_push(&bar_symbol->data.function.parameters, param_y);
    symbol_table_insert(fix->ctx->global, bar_symbol);

    // Call with only 1 arg, but bar expects 2
    ast_expr_t* error_node = ast_call_expr_create_va(ast_ref_expr_create("bar"),
        ast_int_lit_val(42),
        nullptr
    );
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_expr_stmt_create(error_node),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    compiler_error_t* error = vec_get(AST_NODE(error_node)->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "takes 2 arguments"));

    ast_node_destroy(foo_fn);
    ast_node_destroy(bar_fn);
}

// Calling a function with wrong argument type
TEST(ut_sema_fn_fixture_t, call_expr_arg_type_mismatch_error)
{
    ast_decl_t* param_x = ast_param_decl_create("x", ast_type_builtin(TYPE_BOOL));
    ast_def_t* bar_fn = ast_fn_def_create_va("bar", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(),
        param_x, nullptr
    );

    // Register bar function in global symbol table
    symbol_t* bar_symbol = symbol_create("bar", SYMBOL_FUNCTION, bar_fn);
    bar_symbol->type = ast_type_builtin(TYPE_VOID);
    vec_push(&bar_symbol->data.function.parameters, param_x);
    symbol_table_insert(fix->ctx->global, bar_symbol);

    ast_expr_t* error_node = ast_int_lit_val(42);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_expr_stmt_create(ast_call_expr_create_va(
            ast_ref_expr_create("bar"), error_node,  // Passing int32, but expects bool
        nullptr)),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    compiler_error_t* error = vec_get(AST_NODE(error_node)->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot coerce type"));

    ast_node_destroy(foo_fn);
    ast_node_destroy(bar_fn);
}

// Returning wrong type from function
TEST(ut_sema_fn_fixture_t, return_stmt_type_mismatch_function_return_type_error)
{
    ast_expr_t* error_node = ast_int_lit_val(42);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_BOOL),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(error_node),  // Returning bool, but function returns i32
            nullptr
        ), nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(foo_fn), error_node, "cannot coerce type");

    ast_node_destroy(foo_fn);
}

// Emit error for function with return type but missing return statement
TEST(ut_sema_fn_fixture_t, function_with_return_type_has_path_without_return_error)
{
    ast_def_t* error_node = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), ast_int_lit_val(5))),
            nullptr  // No return statement
        ), nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(error_node), error_node, "missing return");

    ast_node_destroy(error_node);
}

// Allow assigning to a parameter
TEST(ut_sema_fn_fixture_t, assignment_to_parameter_allowed)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("param"), ast_int_lit_val(42))),
        nullptr
    ), ast_param_decl_create("param", ast_type_builtin(TYPE_I32)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}

// Calling an undefined function should produce an error
TEST(ut_sema_fn_fixture_t, call_to_undefined_function_error)
{
    ast_expr_t* error_node = ast_ref_expr_create("con");

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_expr_stmt_create(ast_call_expr_create_va(error_node, nullptr)),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "unknown symbol");

    ast_node_destroy(block);
}

// Verify empty function body with return type
TEST(ut_sema_fn_fixture_t, fn_with_ret_type_no_body)
{
    ast_def_t* error_node = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_empty(),  // Empty body
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(error_node));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "missing return"));

    ast_node_destroy(error_node);
}

TEST(ut_sema_fn_fixture_t, function_parameter_with_void_type_error)
{
    // fn foo(x: void) { }
    ast_decl_t* error_node = ast_param_decl_create("x", ast_type_builtin(TYPE_VOID));
    ast_def_t* foo = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_empty(), error_node, nullptr );

    ASSERT_SEMA_ERROR(AST_NODE(foo), error_node, "cannot instantiate type 'void'");

    ast_node_destroy(foo);
}

// Verify types are considered equal when array type & sizes match
TEST(ut_sema_fn_fixture_t, function_return_array_type_match)
{
    // fn foo(arr: [i32, 5]) -> [i32, 5] {
    //     return arr;
    // }
    ast_def_t* fn_def = ast_fn_def_create_va(
        "foo",
        ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_ref_expr_create("arr")),
            nullptr
        ),
        ast_param_decl_create("arr", ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5))),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(fn_def));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(fn_def);
}

// Verify types are considered not equal when array sizes mismatch
TEST(ut_sema_fn_fixture_t, function_return_array_type_size_mismatch)
{
    // fn foo(arr: [i32, 3]) -> [i32, 5] {
    //     return arr;
    // }
    ast_def_t* fn_def = ast_fn_def_create_va(
        "foo",
        ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(3)),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_ref_expr_create("arr")),
            nullptr
        ),
        ast_param_decl_create("arr", ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5))),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(fn_def));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot coerce type"));

    ast_node_destroy(fn_def);
}
