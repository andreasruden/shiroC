#include "ast/decl/decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_var_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_var_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_var_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
}

// Emit an error when a name in the same scope is redeclared
TEST(ut_sema_var_fixture_t, variable_redeclaration_error)
{
    ast_decl_t* error_node = ast_var_decl_create("my_var", ast_type_builtin(TYPE_F32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("my_var", ast_type_builtin(TYPE_BOOL), nullptr)),
        ast_decl_stmt_create(error_node),
        nullptr
    ), nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "'my_var' already declared");

    ast_node_destroy(foo_fn);
}

// Emit a warning when a name of an outer scope is shadowed
TEST(ut_sema_var_fixture_t, variable_shadowing)
{
    ast_decl_t* warning_node = ast_var_decl_create("x", ast_type_builtin(TYPE_F32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(warning_node),
            nullptr
        ),
        nullptr
    ), nullptr);

    ASSERT_SEMA_WARNING_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), warning_node, "shadow");

    ast_node_destroy(foo_fn);
}

// When variable is read, it must have been initialized before
TEST(ut_sema_var_fixture_t, variable_read_requires_initialization)
{
    ast_expr_t* error_node = ast_ref_expr_create("x");
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_PLUS_ASSIGN, error_node, ast_int_lit_val(42))),
        nullptr
    ), nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "not initialized");

    ast_node_destroy(foo_fn);
}

// Writing to a declared but uninitialized variable should be allowed
TEST(ut_sema_var_fixture_t, variable_write_only_requires_declaration)
{
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_val(42))),
        nullptr
    ), nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(foo_fn));

    ast_node_destroy(foo_fn);
}

// Variable initialized in both branches should be considered initialized after if
TEST(ut_sema_var_fixture_t, variable_init_in_if_both_branches)
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

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(foo_fn));

    ast_node_destroy(foo_fn);
}

// Variable initialized only in then-branch should NOT be considered initialized after if
TEST(ut_sema_var_fixture_t, variable_init_in_if_only_then_branch)
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

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "not initialized");

    ast_node_destroy(foo_fn);
}

// Variable initialized only in then-branch with no else should NOT be considered initialized
TEST(ut_sema_var_fixture_t, variable_init_in_if_no_else_branch)
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

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "not initialized");

    ast_node_destroy(foo_fn);
}

// Variable initialized inside while loop should NOT be considered initialized after loop
TEST(ut_sema_var_fixture_t, variable_init_in_while_loop)
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

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "not initialized");

    ast_node_destroy(foo_fn);
}

// Variable initialized before if-statement should remain initialized after
TEST(ut_sema_var_fixture_t, variable_init_before_if_remains_initialized)
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

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(foo_fn));

    ast_node_destroy(foo_fn);
}

// Emit error when variable used in while-condition is not initialized
TEST(ut_sema_var_fixture_t, uninitialized_variable_used_in_while_condition)
{
    ast_expr_t* error_node = ast_ref_expr_create("y");
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("y", ast_type_builtin(TYPE_BOOL), nullptr)),
        ast_while_stmt_create(error_node, ast_compound_stmt_create_empty()),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "not initialized");

    ast_node_destroy(block);
}

// Variable shadowed and initialized in both branches, but outer variable remains uninitialized
TEST(ut_sema_var_fixture_t, variable_shadowing_does_not_affect_outer_scope_initialization)
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

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "not initialized");

    ast_node_destroy(foo_fn);
}

// Error on using symbol that does not exist
TEST(ut_sema_var_fixture_t, symbol_not_exist)
{
    ast_expr_t* error_node = ast_ref_expr_create("inexistant");
    ast_def_t* main_fn = ast_fn_def_create_va("main", nullptr, ast_compound_stmt_create_va(
        ast_if_stmt_create(ast_bin_op_create(TOKEN_ASSIGN, error_node, ast_int_lit_val(5)),
            ast_compound_stmt_create_empty(), nullptr),
        nullptr
    ), nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(main_fn), error_node, "unknown symbol name 'inexistant'");

    ast_node_destroy(main_fn);
}

// Declaring a local variable with the same name as a parameter should produce an error
TEST(ut_sema_var_fixture_t, local_variable_shadows_parameter_error)
{
    ast_decl_t* error_node = ast_var_decl_create("i", ast_type_builtin(TYPE_I32), nullptr);
    ast_def_t* foo_fn = ast_fn_def_create_va("foo", nullptr,
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(error_node),  // Redeclares parameter 'i'
            nullptr
        ),
        ast_param_decl_create("i", ast_type_builtin(TYPE_I32)), nullptr
    );

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(foo_fn), error_node, "redeclares function parameter");

    ast_node_destroy(foo_fn);
}

// Variable initialized in all paths of inner if, but outer if has no else branch, therefore not initialized afterwards
TEST(ut_sema_var_fixture_t, nested_if_initialization_outer_branch_missing)
{
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

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "not initialized");

    ast_node_destroy(block);
}

// Variable initialized in all paths inside while loop body, but loop might not execute
TEST(ut_sema_var_fixture_t, variable_initialized_in_while_loop_not_guaranteed)
{
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

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "not initialized");

    ast_node_destroy(block);
}

// Error if inferred and annotated types disagree
TEST(ut_sema_var_fixture_t, var_decl_type_annotation_and_inference_disagree_error)
{
    ast_decl_t* var_decl = ast_var_decl_create("i", ast_type_builtin(TYPE_I8), ast_int_lit_val(42));

    ast_stmt_t* stmt = ast_decl_stmt_create(var_decl);

    ASSERT_SEMA_ERROR(AST_NODE(stmt), var_decl, "cannot coerce type");

    ast_node_destroy(stmt);
}

// Warn about unnecessary type annotation in variable declaration
TEST(ut_sema_var_fixture_t, var_decl_unnecessary_type_annotation_warning)
{
    ast_decl_t* var_decl = ast_var_decl_create("i", ast_type_builtin(TYPE_I32), ast_int_lit_val(42));

    ast_stmt_t* stmt = ast_decl_stmt_create(var_decl);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(stmt));
    ASSERT_TRUE(res);
    ASSERT_EQ(1, vec_size(&fix->ctx->warning_nodes));
    ast_node_t* offender = vec_get(&fix->ctx->warning_nodes, 0);
    ASSERT_EQ(var_decl, offender);
    compiler_error_t* error = vec_get(offender->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "type annotation is superfluous"));

    ast_node_destroy(stmt);
}

// No warning when the type is necessary
TEST(ut_sema_var_fixture_t, var_decl_necessary_type_annotation_ok)
{
    ast_decl_t* var_decl = ast_var_decl_create("p", ast_type_pointer(ast_type_builtin(TYPE_I32)),
        ast_null_lit_create());

    ast_stmt_t* stmt = ast_decl_stmt_create(var_decl);

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(stmt));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->warning_nodes));

    ast_node_destroy(stmt);
}

TEST(ut_sema_var_fixture_t, variable_declaration_with_void_type_error)
{
    // var x: void;
    ast_decl_t* error_node = ast_var_decl_create("x", ast_type_builtin(TYPE_VOID), nullptr);
    ast_stmt_t* stmt = ast_decl_stmt_create(error_node);

    ASSERT_SEMA_ERROR(AST_NODE(stmt), error_node, "cannot instantiate type 'void'");

    ast_node_destroy(stmt);
}
