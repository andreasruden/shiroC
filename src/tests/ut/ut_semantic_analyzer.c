#include "ast/decl/decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/call_expr.h"
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
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "parser/lexer.h"
#include "sema/init_tracker.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "test_runner.h"

#include <stdarg.h>

TEST_FIXTURE(ut_sema_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    init_tracker_t* init_tracker;  // to allow us analyzing smaller units than functions
};

TEST_SETUP(ut_sema_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    semantic_context_destroy(fix->ctx);
}

// Emit an error when a name in the same scope is redeclared
TEST(ut_sema_fixture_t, variable_redeclaration_error)
{
    ast_decl_t* error_node = ast_var_decl_create("my_var", ast_type_builtin(TYPE_F32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("my_var", ast_type_builtin(TYPE_BOOL), nullptr)),
        ast_decl_stmt_create(error_node),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_FALSE(error->is_warning);
    ASSERT_NEQ(nullptr, strstr(error->description, "'my_var' already declared"));

    ast_node_destroy(foo_fn);
}

// Emit a warnin when a name of an outer scope is shadowed
TEST(ut_sema_fixture_t, variable_shadowing)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_F32), nullptr)),
            nullptr
        ),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));
    ASSERT_EQ(1, vec_size(&fix->ctx->warning_nodes));
    compiler_error_t* warning = vec_get(AST_NODE(vec_get(&fix->ctx->warning_nodes, 0))->errors, 0);
    ASSERT_TRUE(warning->is_warning);
    ASSERT_NEQ(nullptr, strstr(warning->description, "shadow"));

    ast_node_destroy(foo_fn);
}

// When variable is read, it must have been initialized before
TEST(ut_sema_fixture_t, variable_read_requires_initialization)
{
    ast_expr_t* error_node = ast_ref_expr_create("x");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS_ASSIGN, error_node, ast_int_lit_val(42))),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(foo_fn);
}

// Writing to a declared but uninitialized variable should be allowed
TEST(ut_sema_fixture_t, variable_write_only_requires_declaration)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_val(42))),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}

// Function parameters should not emit uninitialized errors
TEST(ut_sema_fixture_t, assume_function_parameter_is_initialized)
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
TEST(ut_sema_fixture_t, call_expr_must_be_ref_function_symbol)
{
    ast_expr_t* error_node = ast_ref_expr_create("not_a_function");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("not_a_function", ast_type_builtin(TYPE_I32),
            ast_int_lit_val(5))),
        ast_expr_stmt_create(ast_call_expr_create_va(error_node, nullptr)),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not callable"));

    ast_node_destroy(foo_fn);
}

// Calling a function with wrong number of arguments
TEST(ut_sema_fixture_t, call_expr_arg_count_mismatch_error)
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
TEST(ut_sema_fixture_t, call_expr_arg_type_mismatch_error)
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
    ASSERT_NEQ(nullptr, strstr(error->description, "parameter 'x' type 'bool'"));

    ast_node_destroy(foo_fn);
    ast_node_destroy(bar_fn);
}

// Returning wrong type from function
TEST(ut_sema_fixture_t, return_stmt_type_mismatch_function_return_type_error)
{
    ast_expr_t* error_node = ast_int_lit_val(42);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_BOOL),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(error_node),  // Returning bool, but function returns i32
            nullptr
        ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "return type"));

    ast_node_destroy(foo_fn);
}

// Emit error for function with return type but missing return statement
TEST(ut_sema_fixture_t, function_with_return_type_has_path_without_return_error)
{
    ast_def_t* error_node = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), ast_int_lit_val(5))),
            nullptr  // No return statement
        ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(error_node));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "missing return"));

    ast_node_destroy(error_node);
}

// If statement with non-boolean condition
TEST(ut_sema_fixture_t, if_condition_must_be_boolean_expr)
{
    ast_expr_t* error_node = ast_int_lit_val(42);

    ast_stmt_t* if_stmt = ast_if_stmt_create(
        error_node,  // Should be boolean
        ast_compound_stmt_create_empty(),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(if_stmt));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "must be bool"));

    ast_node_destroy(if_stmt);
}

// While statement with non-boolean condition
TEST(ut_sema_fixture_t, while_condition_must_be_boolean_expr)
{
    ast_expr_t* error_node = ast_int_lit_val(42);

    ast_stmt_t* while_stmt = ast_while_stmt_create(
        error_node,  // Should be boolean
        ast_compound_stmt_create_empty());

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(while_stmt));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "must be bool"));

    ast_node_destroy(while_stmt);
}

// Assignment with incompatible types
TEST(ut_sema_fixture_t, assignment_with_mismatched_types)
{
    ast_expr_t* error_node = ast_ref_expr_create("x");

    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_BOOL), nullptr)),
        // Error: assign i32 to bool
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(true))),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "type"));

    ast_node_destroy(foo_fn);
}

// Variable initialized in both branches should be considered initialized after if
TEST(ut_sema_fixture_t, variable_init_in_if_both_branches)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_val(23))),
                nullptr
            ),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_val(42))),
                nullptr
            )
        ),
        // After if-statement, i should be initialized
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("i"), ast_int_lit_val(23))),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}

// Variable initialized only in then-branch should NOT be considered initialized after if
TEST(ut_sema_fixture_t, variable_init_in_if_only_then_branch)
{
    ast_expr_t* error_node = ast_ref_expr_create("i");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_val(23))),
                // Inside then-branch, i IS initialized, so this should be OK
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("i"), ast_int_lit_val(5))),
                nullptr
            ),
            ast_compound_stmt_create_va(
                // Else-branch doesn't initialize i
                ast_expr_stmt_create(ast_int_lit_val(50)),
                nullptr
            )
        ),
        // After if-statement, i is NOT initialized (error)
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_int_lit_val(5), error_node)),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(foo_fn);
}

// Variable initialized only in then-branch with no else should NOT be considered initialized
TEST(ut_sema_fixture_t, variable_init_in_if_no_else_branch)
{
    ast_expr_t* error_node = ast_ref_expr_create("i");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_val(23)
                )),
                nullptr
            ),
            nullptr  // No else branch
        ),
        // After if-statement, i might not be initialized (error)
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_STAR, error_node, ast_int_lit_val(5))),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(foo_fn);
}

// Variable initialized inside while loop should NOT be considered initialized after loop
TEST(ut_sema_fixture_t, variable_init_in_while_loop)
{
    ast_expr_t* error_node = ast_ref_expr_create("i");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr)),
        ast_while_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_val(23))),
                nullptr
            )
        ),
        // After while loop, i is NOT guaranteed to be initialized
        ast_expr_stmt_create(error_node),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)),  nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(foo_fn);
}

// Variable initialized before if-statement should remain initialized after
TEST(ut_sema_fixture_t, variable_init_before_if_remains_initialized)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(10))),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                // Do something else in if
                ast_expr_stmt_create(ast_int_lit_val(1)),
                nullptr
            ),
            nullptr
        ),
        // i should still be initialized here
        ast_expr_stmt_create(ast_ref_expr_create("i")),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}

// Emit error when variable used in while-condition is not initialized
TEST(ut_sema_fixture_t, uninitialized_variable_used_in_while_condition)
{
    ast_expr_t* error_node = ast_ref_expr_create("y");
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("y", ast_type_builtin(TYPE_BOOL), nullptr)),
        ast_while_stmt_create(error_node, ast_compound_stmt_create_empty()),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(block);
}

// Variable shadowed and initialized in both branches, but outer variable remains uninitialized
TEST(ut_sema_fixture_t, variable_shadowing_does_not_affect_outer_scope_initialization)
{
    ast_expr_t* error_node = ast_ref_expr_create("i");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"), ast_compound_stmt_create_va(
                // Shadow outer 'i' with a new local 'i'
                ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(10))),
                // This 'i' refers to the inner variable, which is initialized
                ast_expr_stmt_create(ast_ref_expr_create("i")),
                nullptr
            ),
            ast_compound_stmt_create_va(
                // Shadow outer 'i' with another new local 'i'
                ast_decl_stmt_create(ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(20))),
                // This 'i' also refers to the inner variable
                ast_expr_stmt_create(ast_ref_expr_create("i")),
                nullptr
            )
        ),
        // After if-statement, the outer 'i' is still not initialized
        // The initializations only affected the shadowed inner variables
        ast_expr_stmt_create(error_node),
        nullptr
    ), ast_param_decl_create("cond", ast_type_builtin(TYPE_BOOL)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(foo_fn);
}

// Emit error when we try to assign to a function
TEST(ut_sema_fixture_t, assignment_to_function_error)
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

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not l-value"));

    ast_node_destroy(expr);
    ast_node_destroy(foo_fn);
}

// Emit error when we try to assign to a binary-expression
TEST(ut_sema_fixture_t, assignment_to_non_lvalue_expression_error)
{
    // Try to assign to a binary expression (5 * 3 = 30)
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_STAR, ast_int_lit_val(5), ast_int_lit_val(3));;
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(30));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not l-value"));

    ast_node_destroy(expr);
}

// Emit error when we try to assign to a literal
TEST(ut_sema_fixture_t, assignment_to_literal_error)
{
    // Try to assign to a literal (42 = 10)
    ast_expr_t* error_node = ast_int_lit_val(42);
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(10));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not l-value"));

    ast_node_destroy(expr);
}

// Allow assigning to a parameter
TEST(ut_sema_fixture_t, assignment_to_parameter_allowed)
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

// Error on using symbol that does not exist
TEST(ut_sema_fixture_t, symbol_not_exist)
{
    ast_def_t* main_fn = ast_fn_def_create_va("main", nullptr, ast_compound_stmt_create_va(
        ast_if_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("inexistant"), ast_int_lit_val(5)),
            ast_compound_stmt_create_empty(), nullptr),
        nullptr
    ), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(main_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "unknown symbol name 'inexistant'"));

    ast_node_destroy(main_fn);
}

// Comparison is valid in if-condition
TEST(ut_sema_fixture_t, comparison_in_if_condition)
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

// Declaring a local variable with the same name as a parameter should produce an error
TEST(ut_sema_fixture_t, local_variable_shadows_parameter_error)
{
    ast_decl_t* error_node = ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr,
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(error_node),  // Redeclares parameter 'i'
            nullptr
        ),
        ast_param_decl_create("i", ast_type_builtin(TYPE_I32)), nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "redeclares function parameter"));

    ast_node_destroy(foo_fn);
}

// Test that various invalid combinations of integer literals and suffixes get rejected
TEST(ut_sema_fixture_t, reject_invalid_int_literals)
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
TEST(ut_sema_fixture_t, accept_valid_int_literals)
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
TEST(ut_sema_fixture_t, reject_float_literal_overflow_f32)
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
TEST(ut_sema_fixture_t, accept_valid_float_literals)
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

TEST(ut_sema_fixture_t, accept_assign_address_to_pointer)
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

TEST(ut_sema_fixture_t, reject_assign_value_to_pointer)
{
    // var i = 10;
    // var ptr: i32*;
    // ptr = i;  // Error: can't assign i32 to i32*
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(10))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_I32)), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("ptr"), ast_ref_expr_create("i"))),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    compiler_error_t* error = vec_get(((ast_node_t*)vec_get(&fix->ctx->error_nodes, 0))->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "type 'i32*'"));

    ast_node_destroy(AST_NODE(block));
}

TEST(ut_sema_fixture_t, accept_dereference_pointer)
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

TEST(ut_sema_fixture_t, reject_assign_to_address_of_lvalue)
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

TEST(ut_sema_fixture_t, reject_null_without_type_annotation)
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

TEST(ut_sema_fixture_t, accept_null_with_type_annotation)
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

TEST(ut_sema_fixture_t, accept_null_assigned_to_non_pointer_type)
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

TEST(ut_sema_fixture_t, accept_null_comparison_in_function)
{
    // foo(ptr: bool*) { if (ptr == null) {} }
    ast_def_t* func = ast_fn_def_create_va(
        "foo", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_va(
            ast_if_stmt_create(ast_bin_op_create(TOKEN_EQ, ast_ref_expr_create("ptr"), ast_null_lit_create()),
                ast_compound_stmt_create_empty(), nullptr), nullptr),
        ast_param_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_BOOL))),
        nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(func));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(AST_NODE(func));
}

// Arithmetic operations on boolean operands should produce an error
TEST(ut_sema_fixture_t, arithmetic_operation_on_booleans_error)
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
TEST(ut_sema_fixture_t, assign_to_lvalue_deref)
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

// Verify ptr is assumed to be initialized with null
TEST(ut_sema_fixture_t, initialize_ptr_with_null)
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

// Verify empty function body with return type
TEST(ut_sema_fixture_t, fn_with_ret_type_no_body)
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

// Variable initialized in all paths of inner if, but outer if has no else branch, therefore initialized afterwards
TEST(ut_sema_fixture_t, nested_if_initialization_outer_branch_missing)
{
    // var ptr: i32*;
    // var i = 42;
    // if (i > 30) {
    //     if (i < 60) {
    //         ptr = &i;
    //     } else {
    //         ptr = &i;
    //     }
    // }
    // ptr;  // error: not guaranteed to be initialized
    ast_expr_t* error_node = ast_ref_expr_create("ptr");
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_I32)),
            nullptr)),
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr,
            ast_int_lit_val(42))),
        ast_if_stmt_create(
            ast_bin_op_create(TOKEN_GT, ast_ref_expr_create("i"), ast_int_lit_val(30)),
            ast_compound_stmt_create_va(
                // Inner if with both branches initializing ptr
                ast_if_stmt_create(
                    ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(60)),
                    ast_compound_stmt_create_va(
                        ast_expr_stmt_create(ast_bin_op_create(
                            TOKEN_ASSIGN,
                            ast_ref_expr_create("ptr"),
                            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i"))
                        )),
                        nullptr
                    ),
                    ast_compound_stmt_create_va(
                        ast_expr_stmt_create(ast_bin_op_create(
                            TOKEN_ASSIGN,
                            ast_ref_expr_create("ptr"),
                            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i"))
                        )),
                        nullptr
                    )
                ),
                nullptr
            ),
            nullptr  // No else branch for outer if
        ),
        // ptr is NOT guaranteed to be initialized here
        ast_expr_stmt_create(error_node),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(block);
}

// Variable initialized in all paths inside while loop body, but loop might not execute
TEST(ut_sema_fixture_t, variable_initialized_in_while_loop_not_guaranteed)
{
    // var i = 40;
    // var j;
    // while (i < 20) {
    //     if (i < 40) {
    //         j = 20;
    //     } else {
    //         j = 30;
    //     }
    // }
    // j; // error: not guaranteed to be initialized
    ast_expr_t* error_node = ast_ref_expr_create("j");
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(40))),
        ast_decl_stmt_create(ast_var_decl_create("j", ast_type_builtin(TYPE_I32), nullptr)),
        ast_while_stmt_create(
            ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(20)),
            ast_compound_stmt_create_va(
                ast_if_stmt_create(
                    ast_bin_op_create(TOKEN_LT, ast_ref_expr_create("i"), ast_int_lit_val(40)),
                    ast_compound_stmt_create_va(
                        ast_expr_stmt_create(ast_bin_op_create(
                            TOKEN_ASSIGN,
                            ast_ref_expr_create("j"),
                            ast_int_lit_val(20)
                        )),
                        nullptr
                    ),
                    ast_compound_stmt_create_va(
                        ast_expr_stmt_create(ast_bin_op_create(
                            TOKEN_ASSIGN,
                            ast_ref_expr_create("j"),
                            ast_int_lit_val(30)
                        )),
                        nullptr
                    )
                ),
                nullptr
            )
        ),
        // j is NOT guaranteed to be initialized (loop might not execute)
        ast_expr_stmt_create(error_node),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "not initialized"));

    ast_node_destroy(block);
}
