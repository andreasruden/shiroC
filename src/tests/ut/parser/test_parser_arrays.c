#include "ast/decl/var_decl.h"
#include "ast/expr/array_lit.h"
#include "ast/expr/array_slice.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/type.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_arrays_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_arrays_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_arrays_fixture_t)
{
    parser_destroy(fix->parser);
    ast_type_cache_reset();
}

TEST(parser_arrays_fixture_t, parse_type_annotation_fixed_array)
{
    parser_set_source(fix->parser, "test", "var arr: [i32, 5];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("arr", ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)),
        nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_type_annotation_heap_array)
{
    parser_set_source(fix->parser, "test", "var arr: [bool];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("arr", ast_type_heap_array(ast_type_builtin(TYPE_BOOL)),
        nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_type_annotation_view)
{
    parser_set_source(fix->parser, "test", "var arr_view: view[f32*];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("arr_view", ast_type_view(ast_type_pointer(ast_type_builtin(TYPE_F32))),
        nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_array_subscript)
{
    parser_set_source(fix->parser, "test", "arr[i * 5]");

    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_expr_t* expected = ast_array_subscript_create(ast_ref_expr_create("arr"), ast_bin_op_create(TOKEN_STAR,
        ast_ref_expr_create("i"), ast_int_lit_val(5)));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_arrays_fixture_t, parse_array_literal)
{
    parser_set_source(fix->parser, "test", "var arr = [1, 2, 3];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(ast_var_decl_create("arr", nullptr, ast_array_lit_create_va(
        ast_int_lit_val(1), ast_int_lit_val(2), ast_int_lit_val(3), nullptr)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_array_slice_both_bounds)
{
    parser_set_source(fix->parser, "test", "arr[2..5]");

    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_expr_t* expected = ast_array_slice_create(
        ast_ref_expr_create("arr"),
        ast_int_lit_val(2),
        ast_int_lit_val(5));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_arrays_fixture_t, parse_multi_dimensional_array_type)
{
    parser_set_source(fix->parser, "test", "var multi_dim: [[i32, 2], 3];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Create inner array type: [i32, 2]
    ast_type_t* inner_array_type = ast_type_array_size_unresolved(
        ast_type_builtin(TYPE_I32),
        ast_int_lit_val(2));

    // Create outer array type: [[i32, 2], 3]
    ast_type_t* outer_array_type = ast_type_array_size_unresolved(
        inner_array_type,
        ast_int_lit_val(3));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("multi_dim", outer_array_type, nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_multi_dimensional_array_literal)
{
    parser_set_source(fix->parser, "test", "var multi_dim = [[1, 2], [3, 4], [5, 6]];");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Create array literal: [[1, 2], [3, 4], [5, 6]]
    ast_expr_t* array_literal = ast_array_lit_create_va(
        ast_array_lit_create_va(ast_int_lit_val(1), ast_int_lit_val(2), nullptr),
        ast_array_lit_create_va(ast_int_lit_val(3), ast_int_lit_val(4), nullptr),
        ast_array_lit_create_va(ast_int_lit_val(5), ast_int_lit_val(6), nullptr),
        nullptr
    );

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("multi_dim", nullptr, array_literal));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_arrays_fixture_t, parse_multi_dimensional_array_subscript)
{
    parser_set_source(fix->parser, "test", "multi_dim[2][1]");

    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Create multi_dim[2][1] expression
    ast_expr_t* expected = ast_array_subscript_create(
        ast_array_subscript_create(
            ast_ref_expr_create("multi_dim"),
            ast_int_lit_val(2)
        ),
        ast_int_lit_val(1)
    );

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}
