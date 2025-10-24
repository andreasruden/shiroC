#include "ast/decl/decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/array_lit.h"
#include "ast/expr/array_slice.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/uninit_lit.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_array_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
    init_tracker_t* init_tracker;  // to allow us analyzing smaller units than functions
};

TEST_SETUP(ut_sema_array_fixture_t)
{
    fix->ctx = semantic_context_create("test", "sema_array");
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_array_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
    ast_type_cache_reset();
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
    ast_decl_t* error_node = ast_var_decl_create("arr",
        nullptr,  // No type annotation
        ast_array_lit_create_empty());

    ast_stmt_t* block = ast_compound_stmt_create_va(ast_decl_stmt_create(error_node), nullptr);

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

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot coerce type");

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

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot coerce type");

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

TEST(ut_sema_array_fixture_t, array_subscript_index_must_be_integer)
{
    // var arr: [i32, 3] = [1, 2, 3];
    // arr[true];  // Error: index must be integer type
    ast_expr_t* error_node = ast_array_subscript_create(
        ast_ref_expr_create("arr"),
        ast_bool_lit_create(true)
    );

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(3)),
            ast_array_lit_create_va(ast_int_lit_val(1), ast_int_lit_val(2), ast_int_lit_val(3), nullptr))),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "'bool' is not usable as an index");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_subscript_non_array_type_error)
{
    // var x: i32 = 42;
    // x[0];  // Error: cannot subscript non-array type
    ast_expr_t* error_node = ast_array_subscript_create(
        ast_ref_expr_create("x"),
        ast_int_lit_val(0)
    );

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x",
            ast_type_builtin(TYPE_I32), ast_int_lit_val(42))),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "subscript");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_subscript_constant_out_of_bounds_error)
{
    // var arr: [i32, 5] = [1, 2, 3, 4, 5];
    // arr[5];  // Error: index 5 out of bounds for array of size 5
    ast_expr_t* error_node = ast_array_subscript_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(5)
    );

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)),
            ast_array_lit_create_va(ast_int_lit_val(1), ast_int_lit_val(2), ast_int_lit_val(3), ast_int_lit_val(4),
            ast_int_lit_val(5), nullptr))),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "index '5' is out of bounds for '[i32, 5]'");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_subscript_returns_element_type)
{
    // var arr = [0, 0, 0];
    // arr[0] = 5;
    // var x: i32 = arr[0];  // OK: arr[0] has type i32
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr", nullptr, ast_array_lit_create_va(
            ast_int_lit_val(0), ast_int_lit_val(0), ast_int_lit_val(0), nullptr))),
        ast_expr_stmt_create(ast_bin_op_create(
            TOKEN_ASSIGN,
            ast_array_subscript_create(
                ast_ref_expr_create("arr"),
                ast_int_lit_val(0)
            ),
            ast_int_lit_val(5)
        )),
        ast_decl_stmt_create(ast_var_decl_create("x",
            ast_type_builtin(TYPE_I32),
            ast_array_subscript_create(
                ast_ref_expr_create("arr"),
                ast_int_lit_val(0)
            ))),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_subscript_uninitialized_array_error)
{
    // var arr: [i32, 3];
    // arr[0];  // Error: array not initialized
    ast_expr_t* error_node = ast_ref_expr_create("arr");

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(3)),
            nullptr)),
        ast_expr_stmt_create(ast_array_subscript_create(
            error_node,
            ast_int_lit_val(0)
        )),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "not initialized");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_uninit_with_type_annotation_allows_subscript)
{
    // var arr: [i32, 5] = uninit;
    // arr[0];  // OK: uninit explicitly marks array as uninitialized, no error
    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)),
            ast_uninit_lit_create())),
        ast_expr_stmt_create(ast_array_subscript_create(
            ast_ref_expr_create("arr"),
            ast_int_lit_val(0)
        )),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_uninit_without_type_annotation_error)
{
    // var bad_arr = uninit;  // Error: cannot infer type from uninit
    ast_decl_t* error_node = ast_var_decl_create("bad_arr",
        nullptr,  // No type annotation
        ast_uninit_lit_create());

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "missing type");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_returns_view_type)
{
    // var arr: [i32, 10] = uninit;
    // var slice: view[i32] = arr[2..5];
    ast_expr_t* slice_expr = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(2),
        ast_int_lit_val(5));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(10)),
            ast_uninit_lit_create())),
        ast_decl_stmt_create(ast_var_decl_create("slice",
            ast_type_view(ast_type_builtin(TYPE_I32)),
            slice_expr)),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    // Verify slice expression has view type
    ASSERT_EQ(slice_expr->type, ast_type_view(ast_type_builtin(TYPE_I32)));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_requires_array_or_view)
{
    // var x: i32 = 42;
    // x[2..5];  // Error: cannot slice non-array type
    ast_expr_t* error_node = ast_array_slice_create(
        ast_ref_expr_create("x"),
        ast_int_lit_val(2),
        ast_int_lit_val(5));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("x",
            ast_type_builtin(TYPE_I32),
            ast_int_lit_val(42))),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot slice");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_bounds_must_be_integer)
{
    // var arr: [i32, 10] = uninit;
    // arr[true..5];  // Error: slice bounds must be integer
    ast_expr_t* error_node = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_bool_lit_create(true),
        ast_int_lit_val(5));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(10)),
            ast_uninit_lit_create())),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "not usable for bounds");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_start_greater_than_end_error)
{
    // var arr: [i32, 10] = uninit;
    // arr[5..2];  // Error: start index greater than end index
    ast_expr_t* error_node = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(5),
        ast_int_lit_val(2));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(10)),
            ast_uninit_lit_create())),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "invalid slice bounds");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_constant_out_of_bounds_error)
{
    // var arr: [i32, 5] = uninit;
    // arr[2..10];  // Error: end index out of bounds
    ast_expr_t* error_node = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(2),
        ast_int_lit_val(10));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)),
            ast_uninit_lit_create())),
        ast_expr_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "out of bounds");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_slice_element_is_lvalue)
{
    // var arr: [i32, 10] = uninit;
    // arr[2..5][0] = 42;  // OK: subscripting a view returns lvalue
    ast_expr_t* slice_expr = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(2),
        ast_int_lit_val(5));

    ast_expr_t* subscript_expr = ast_array_subscript_create(
        slice_expr,
        ast_int_lit_val(0));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(10)),
            ast_uninit_lit_create())),
        ast_expr_stmt_create(ast_bin_op_create(TOKEN_ASSIGN,
            subscript_expr,
            ast_int_lit_val(42))),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, multi_dimensional_array_declaration_and_subscript)
{
    // var multi_dim: [[i32, 2], 3] = [[1, 2], [3, 4], [5, 6]];
    // multi_dim[2][1];

    // Create inner array type: [i32, 2]
    ast_type_t* inner_array_type = ast_type_array_size_unresolved(
        ast_type_builtin(TYPE_I32),
        ast_int_lit_val(2));

    // Create outer array type: [[i32, 2], 3]
    ast_type_t* outer_array_type = ast_type_array_size_unresolved(
        inner_array_type,
        ast_int_lit_val(3));

    // Create array literal: [[1, 2], [3, 4], [5, 6]]
    ast_expr_t* array_literal = ast_array_lit_create_va(
        ast_array_lit_create_va(ast_int_lit_val(1), ast_int_lit_val(2), nullptr),
        ast_array_lit_create_va(ast_int_lit_val(3), ast_int_lit_val(4), nullptr),
        ast_array_lit_create_va(ast_int_lit_val(5), ast_int_lit_val(6), nullptr),
        nullptr
    );

    // Create multi_dim[2][1] expression
    ast_expr_t* first_subscript = ast_array_subscript_create(
        ast_ref_expr_create("multi_dim"),
        ast_int_lit_val(2)
    );

    ast_expr_t* second_subscript = ast_array_subscript_create(
        first_subscript,
        ast_int_lit_val(1)
    );

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("multi_dim",
            outer_array_type,
            array_literal)),
        ast_expr_stmt_create(second_subscript),
        nullptr
    );

    bool res = semantic_analyzer_run(fix->sema, AST_NODE(block));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    // Verify that first_subscript has type [i32, 2]
    ASSERT_EQ(first_subscript->type->kind, AST_TYPE_ARRAY);
    ASSERT_EQ(first_subscript->type, ast_type_array(ast_type_builtin(TYPE_I32), 2));

    // Verify that second_subscript has type i32
    ASSERT_EQ(second_subscript->type, ast_type_builtin(TYPE_I32));

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, view_from_array_literal_error)
{
    // var bad: view[i32] = [1, 2, 3];  // Error: cannot create view from array literal
    ast_decl_t* error_node = ast_var_decl_create("bad",
        ast_type_view(ast_type_builtin(TYPE_I32)),
        ast_array_lit_create_va(
            ast_int_lit_val(1),
            ast_int_lit_val(2),
            ast_int_lit_val(3),
            nullptr
        ));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(error_node),
        nullptr
    );

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot create view into array literal");

    ast_node_destroy(block);
}

TEST(ut_sema_array_fixture_t, array_to_array_assignment_error)
{
    // fn foo() -> [i32, 3] {
    //     var arr: [i32, 3] = [1, 2, 3];
    //     return arr;  // Error: cannot assign array to array
    // }
    ast_expr_t* error_node = ast_ref_expr_create("arr");

    ast_stmt_t* body = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr",
            ast_type_array(ast_type_builtin(TYPE_I32), 3),
            ast_array_lit_create_va(
                ast_int_lit_val(1),
                ast_int_lit_val(2),
                ast_int_lit_val(3),
                nullptr
            ))),
        ast_return_stmt_create(error_node),
        nullptr
    );

    ast_def_t* fn_def = ast_fn_def_create_va("foo",
        ast_type_array(ast_type_builtin(TYPE_I32), 3),
        body,
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(fn_def), error_node, "cannot assign array");

    ast_node_destroy(fn_def);
}
