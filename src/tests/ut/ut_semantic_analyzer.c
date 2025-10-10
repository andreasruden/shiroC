#include "ast/decl/decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
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
#include "lexer.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "test_runner.h"

#include <stdarg.h>

TEST_FIXTURE(ut_sema_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
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
    ast_decl_t* error_node = ast_var_decl_create("my_var", ast_type_from_builtin(TYPE_F32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("my_var", ast_type_from_builtin(TYPE_BOOL), nullptr)),
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
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_F32), nullptr)),
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
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS_ASSIGN, error_node, ast_int_lit_create(42))),
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
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_create(42))),
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
    ast_decl_t* param = ast_param_decl_create("param", ast_type_from_builtin(TYPE_I32));
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
            ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("param"), ast_int_lit_create(42))),
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
        ast_decl_stmt_create(ast_var_decl_create("not_a_function", ast_type_from_builtin(TYPE_I32),
            ast_int_lit_create(5))),
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
    ast_decl_t* param_x = ast_param_decl_create("x", ast_type_from_builtin(TYPE_I32));
    ast_decl_t* param_y = ast_param_decl_create("y", ast_type_from_builtin(TYPE_I32));
    ast_def_t* bar_fn = ast_fn_def_create_va("bar", ast_type_from_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(),
        param_x, param_y, nullptr
    );

    // Register bar function in global symbol table
    symbol_t* bar_symbol = symbol_create("bar", SYMBOL_FUNCTION, bar_fn);
    bar_symbol->type = ast_type_from_builtin(TYPE_VOID);
    vec_push(&bar_symbol->data.function.parameters, param_x);
    vec_push(&bar_symbol->data.function.parameters, param_y);
    symbol_table_insert(fix->ctx->global, bar_symbol);

    // Call with only 1 arg, but bar expects 2
    ast_expr_t* error_node = ast_call_expr_create_va(ast_ref_expr_create("bar"),
        ast_int_lit_create(42),
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
    ast_decl_t* param_x = ast_param_decl_create("x", ast_type_from_builtin(TYPE_BOOL));
    ast_def_t* bar_fn = ast_fn_def_create_va("bar", ast_type_from_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(),
        param_x, nullptr
    );

    // Register bar function in global symbol table
    symbol_t* bar_symbol = symbol_create("bar", SYMBOL_FUNCTION, bar_fn);
    bar_symbol->type = ast_type_from_builtin(TYPE_VOID);
    vec_push(&bar_symbol->data.function.parameters, param_x);
    symbol_table_insert(fix->ctx->global, bar_symbol);

    ast_expr_t* error_node = ast_int_lit_create(42);
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
    ast_expr_t* error_node = ast_int_lit_create(42);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", ast_type_from_builtin(TYPE_BOOL),
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
    ast_def_t* error_node = ast_fn_def_create_va("foo", ast_type_from_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), ast_int_lit_create(5))),
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
    ast_expr_t* error_node = ast_int_lit_create(42);

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
    ast_expr_t* error_node = ast_int_lit_create(42);

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
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_from_builtin(TYPE_BOOL), nullptr)),
        // Error: assign i32 to bool
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_create(true))),
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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_create(23))),
                nullptr
            ),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_create(42))),
                nullptr
            )
        ),
        // After if-statement, i should be initialized
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("i"), ast_int_lit_create(23))),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)), nullptr);

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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_create(23))),
                // Inside then-branch, i IS initialized, so this should be OK
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_ref_expr_create("i"), ast_int_lit_create(5))),
                nullptr
            ),
            ast_compound_stmt_create_va(
                // Else-branch doesn't initialize i
                ast_expr_stmt_create(ast_int_lit_create(50)),
                nullptr
            )
        ),
        // After if-statement, i is NOT initialized (error)
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS, ast_int_lit_create(5), error_node)),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)), nullptr);

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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_create(23)
                )),
                nullptr
            ),
            nullptr  // No else branch
        ),
        // After if-statement, i might not be initialized (error)
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_STAR, error_node, ast_int_lit_create(5))),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)), nullptr);

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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_while_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("i"), ast_int_lit_create(23))),
                nullptr
            )
        ),
        // After while loop, i is NOT guaranteed to be initialized
        ast_expr_stmt_create(error_node),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)),  nullptr);

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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), ast_int_lit_create(10))),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"),
            ast_compound_stmt_create_va(
                // Do something else in if
                ast_expr_stmt_create(ast_int_lit_create(1)),
                nullptr
            ),
            nullptr
        ),
        // i should still be initialized here
        ast_expr_stmt_create(ast_ref_expr_create("i")),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)), nullptr);

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
        ast_decl_stmt_create(ast_var_decl_create("y", ast_type_from_builtin(TYPE_BOOL), nullptr)),
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
        ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), nullptr)),
        ast_if_stmt_create(
            ast_ref_expr_create("cond"), ast_compound_stmt_create_va(
                // Shadow outer 'i' with a new local 'i'
                ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), ast_int_lit_create(10))),
                // This 'i' refers to the inner variable, which is initialized
                ast_expr_stmt_create(ast_ref_expr_create("i")),
                nullptr
            ),
            ast_compound_stmt_create_va(
                // Shadow outer 'i' with another new local 'i'
                ast_decl_stmt_create(ast_var_decl_create("i", ast_type_from_builtin(TYPE_I32), ast_int_lit_create(20))),
                // This 'i' also refers to the inner variable
                ast_expr_stmt_create(ast_ref_expr_create("i")),
                nullptr
            )
        ),
        // After if-statement, the outer 'i' is still not initialized
        // The initializations only affected the shadowed inner variables
        ast_expr_stmt_create(error_node),
        nullptr
    ), ast_param_decl_create("cond", ast_type_from_builtin(TYPE_BOOL)), nullptr);

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
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", ast_type_from_builtin(TYPE_VOID), ast_compound_stmt_create_empty(),
        nullptr);
    symbol_t* foo_symbol = symbol_create("foo", SYMBOL_FUNCTION, foo_fn);
    foo_symbol->type = ast_type_from_builtin(TYPE_VOID);
    symbol_table_insert(fix->ctx->global, foo_symbol);

    // Try to assign to the function
    ast_expr_t* error_node = ast_ref_expr_create("foo");
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_create(12));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot be assigned to"));

    ast_node_destroy(expr);
    ast_node_destroy(foo_fn);
}

// Emit error when we try to assign to a binary-expression
TEST(ut_sema_fixture_t, assignment_to_non_lvalue_expression_error)
{
    // Try to assign to a binary expression (5 * 3 = 30)
    ast_expr_t* error_node = ast_bin_op_create(TOKEN_STAR, ast_int_lit_create(5), ast_int_lit_create(3));;
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_create(30));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot be assigned to"));

    ast_node_destroy(expr);
}

// Emit error when we try to assign to a literal
TEST(ut_sema_fixture_t, assignment_to_literal_error)
{
    // Try to assign to a literal (42 = 10)
    ast_expr_t* error_node = ast_int_lit_create(42);
    ast_expr_t* expr = ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_create(10));

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(expr));
    ASSERT_FALSE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "cannot be assigned to"));

    ast_node_destroy(expr);
}

// Allow assigning to a parameter
TEST(ut_sema_fixture_t, assignment_to_parameter_allowed)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("param"), ast_int_lit_create(42))),
        nullptr
    ), ast_param_decl_create("param", ast_type_from_builtin(TYPE_I32)), nullptr);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(foo_fn));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(foo_fn);
}
