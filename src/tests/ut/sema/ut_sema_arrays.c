#include "ast/decl/decl.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_array_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    init_tracker_t* init_tracker;  // to allow us analyzing smaller units than functions
};

TEST_SETUP(ut_sema_array_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_array_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    semantic_context_destroy(fix->ctx);
}

TEST(ut_sema_array_fixture_t, array_literal_type_inference_basic)
{
    // var arr = [1, 2, 3];  // Infer type: [i32, 3]
    ast_decl_t* var_decl = ast_var_decl_create("arr",
        nullptr,  // No type annotation - should infer
        ast_array_lit_create_va(
            ast_int_lit_val(1),
            ast_int_lit_val(2),
            ast_int_lit_val(3),
            nullptr
        ));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(var_decl),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    // Verify the inferred type is [i32, 3]
    // ASSERT_EQ(var_decl->type, ast_type_array(ast_type_builtin(TYPE_I32), 3));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_literal_empty_requires_type_annotation)
{
    // var arr = [];  // Error: cannot infer type of empty array
    ast_expr_t* error_node = ast_array_lit_create_empty();

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            nullptr,  // No type annotation
            error_node)),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot infer");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_literal_mixed_types_error)
{
    // var arr = [1, true, 3];  // Error: mixed types in array literal
    ast_expr_t* error_node = ast_array_lit_create_va(
        ast_int_lit_val(1),
        ast_bool_lit_create(true),
        ast_int_lit_val(3),
        nullptr
    );

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            nullptr,
            error_node)),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "mixed types");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_literal_element_count_mismatch_error)
{
    // var arr: [i32, 3] = [1, 2];  // Error: expected 3 elements, got 2
    ast_decl_t* error_node = ast_var_decl_create("arr",
        ast_type_array(ast_type_builtin(TYPE_I32), 3),
        ast_array_lit_create_va(
            ast_int_lit_val(1),
            ast_int_lit_val(2),
        nullptr));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "inferred and annotated types differ");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_literal_element_type_mismatch_error)
{
    // var arr: [bool, 3] = [1, 2, 3];  // Error: expected bool, got i32
    ast_decl_t* error_node = ast_var_decl_create("arr",
        ast_type_array(ast_type_builtin(TYPE_BOOL), 3),
        ast_array_lit_create_va(
            ast_int_lit_val(1),
            ast_int_lit_val(2),
            ast_int_lit_val(3),
        nullptr));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "inferred and annotated types differ");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_literal_assignment_to_existing_variable)
{
    // var arr: [i32, 3];
    // arr = [1, 2, 3];     // OK
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array(ast_type_builtin(TYPE_I32), 3), nullptr)),
        ast_expr_stmt_create(ast_bin_op_create(
            TOKEN_ASSIGN,
            ast_ref_expr_create("arr"),
            ast_array_lit_create_va(
                ast_int_lit_val(1),
                ast_int_lit_val(2),
                ast_int_lit_val(3),
                nullptr
            ))),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}