#include "ast/expr/access_expr.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/cast_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_expressions_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_expressions_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_expressions_fixture_t)
{
    parser_destroy(fix->parser);
    ast_type_cache_reset();
}

TEST(parser_expressions_fixture_t, parse_basic_bin_op)
{
    parser_set_source(fix->parser, "test", "var1 == var2");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected expression
    ast_expr_t* expected = ast_bin_op_create( TOKEN_EQ, ast_ref_expr_create("var1"), ast_ref_expr_create("var2"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_left_associative_same_precedence)
{
    parser_set_source(fix->parser, "test", "a + b + c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a + b) + c
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_bin_op_create(TOKEN_PLUS,
            ast_ref_expr_create("a"),
            ast_ref_expr_create("b")),
        ast_ref_expr_create("c"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_precedence_mul_before_add)
{
    parser_set_source(fix->parser, "test", "a + b * c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: a + (b * c)
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_ref_expr_create("a"),
        ast_bin_op_create(TOKEN_STAR,
            ast_ref_expr_create("b"),
            ast_ref_expr_create("c")));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_precedence_add_then_mul)
{
    parser_set_source(fix->parser, "test", "a * b + c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a * b) + c
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_bin_op_create(TOKEN_STAR,
            ast_ref_expr_create("a"),
            ast_ref_expr_create("b")),
        ast_ref_expr_create("c"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_multiple_precedence_levels)
{
    parser_set_source(fix->parser, "test", "a + b * c + d");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a + (b * c)) + d
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_bin_op_create(TOKEN_PLUS,
            ast_ref_expr_create("a"),
            ast_bin_op_create(TOKEN_STAR,
                ast_ref_expr_create("b"),
                ast_ref_expr_create("c"))),
        ast_ref_expr_create("d"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_parentheses_override_precedence)
{
    parser_set_source(fix->parser, "test", "(a + b) * c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a + b) * c
    ast_expr_t* expected = ast_bin_op_create(TOKEN_STAR,
        ast_paren_expr_create(
            ast_bin_op_create(TOKEN_PLUS,
                ast_ref_expr_create("a"),
                ast_ref_expr_create("b"))),
        ast_ref_expr_create("c"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_nested_parentheses)
{
    parser_set_source(fix->parser, "test", "((a + b) * c) + d");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: ((a + b) * c) + d
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_paren_expr_create(
            ast_bin_op_create(TOKEN_STAR,
                ast_paren_expr_create(
                    ast_bin_op_create(TOKEN_PLUS,
                        ast_ref_expr_create("a"),
                        ast_ref_expr_create("b"))),
                ast_ref_expr_create("c"))),
        ast_ref_expr_create("d"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_comparison_with_arithmetic)
{
    parser_set_source(fix->parser, "test", "a + b == c * d");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a + b) == (c * d)
    ast_expr_t* expected = ast_bin_op_create(TOKEN_EQ,
        ast_bin_op_create(TOKEN_PLUS,
            ast_ref_expr_create("a"),
            ast_ref_expr_create("b")),
        ast_bin_op_create(TOKEN_STAR,
            ast_ref_expr_create("c"),
            ast_ref_expr_create("d")));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_complex_mixed_precedence)
{
    parser_set_source(fix->parser, "test", "a * b + c < d + e * f");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: ((a * b) + c) < (d + (e * f))
    ast_expr_t* expected = ast_bin_op_create(TOKEN_LT,
        ast_bin_op_create(TOKEN_PLUS,
            ast_bin_op_create(TOKEN_STAR,
                ast_ref_expr_create("a"),
                ast_ref_expr_create("b")),
            ast_ref_expr_create("c")),
        ast_bin_op_create(TOKEN_PLUS,
            ast_ref_expr_create("d"),
            ast_bin_op_create(TOKEN_STAR,
                ast_ref_expr_create("e"),
                ast_ref_expr_create("f"))));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_chained_division)
{
    parser_set_source(fix->parser, "test", "a / b / c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: (a / b) / c  (left-to-right)
    ast_expr_t* expected = ast_bin_op_create(TOKEN_DIV,
        ast_bin_op_create(TOKEN_DIV,
            ast_ref_expr_create("a"),
            ast_ref_expr_create("b")),
        ast_ref_expr_create("c"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_mixed_add_subtract)
{
    parser_set_source(fix->parser, "test", "a - b + c - d");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: ((a - b) + c) - d
    ast_expr_t* expected = ast_bin_op_create(TOKEN_MINUS,
        ast_bin_op_create(TOKEN_PLUS,
            ast_bin_op_create(TOKEN_MINUS,
                ast_ref_expr_create("a"),
                ast_ref_expr_create("b")),
            ast_ref_expr_create("c")),
        ast_ref_expr_create("d"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_expressions_fixture_t, parse_simple_assignment)
{
    parser_set_source(fix->parser, "test", "x = 5");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_expr_t* expected = ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_val(5));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_chained_assignment)
{
    parser_set_source(fix->parser, "test", "x += y *= 10");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: x += (y /= 10)
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS_ASSIGN,
        ast_ref_expr_create("x"),
        ast_bin_op_create(TOKEN_MUL_ASSIGN,
            ast_ref_expr_create("y"),
            ast_int_lit_val(10)));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_assignment_with_expression)
{
    parser_set_source(fix->parser, "test", "x = a + b * c");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: x = (a + (b * c))
    ast_expr_t* expected = ast_bin_op_create(TOKEN_ASSIGN,
        ast_ref_expr_create("x"),
        ast_bin_op_create(TOKEN_PLUS,
            ast_ref_expr_create("a"),
            ast_bin_op_create(TOKEN_STAR,
                ast_ref_expr_create("b"),
                ast_ref_expr_create("c"))));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_simple_access_expr)
{
    parser_set_source(fix->parser, "test", "obj.field");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: access_expr(ref("obj"), ref("field"))
    ast_expr_t* expected = ast_access_expr_create(ast_ref_expr_create("obj"), ast_ref_expr_create("field"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_chained_access_expr)
{
    parser_set_source(fix->parser, "test", "obj.inner.value");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: access_expr(access_expr(ref("obj"), ref("inner")), ref("value"))
    ast_expr_t* expected = ast_access_expr_create(
        ast_access_expr_create(ast_ref_expr_create("obj"), ast_ref_expr_create("inner")),
        ast_ref_expr_create("value"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_access_expr_call)
{
    parser_set_source(fix->parser, "test", "X.Y()");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: call_expr(access_expr(ref("X"), ref("Y")))
    ast_expr_t* expected = ast_call_expr_create_va(
        ast_access_expr_create(ast_ref_expr_create("X"), ast_ref_expr_create("Y")),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_expressions_fixture_t, parse_cast_to_pointer)
{
    parser_set_source(fix->parser, "test", "a as i32*");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Should parse as: cast_expr(ref("a"), i32*)
    ast_type_t* i32_ptr_type = ast_type_pointer(ast_type_builtin(TYPE_I32));
    ast_expr_t* expected = ast_cast_expr_create(ast_ref_expr_create("a"), i32_ptr_type);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}
