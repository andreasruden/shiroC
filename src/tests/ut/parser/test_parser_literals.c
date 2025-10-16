#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/node.h"
#include "compiler_error.h"
#include "parser/parser.h"
#include "test_runner.h"

#include <string.h>

TEST_FIXTURE(parser_literals_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_literals_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_literals_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_literals_fixture_t, parse_int_literal)
{
    parser_set_source(fix->parser, "test", "1234567");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_int_lit_t* lit = (ast_int_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_INT_LIT, AST_KIND(lit));
    ASSERT_EQ(1234567, lit->value.magnitude);
    ASSERT_FALSE(lit->has_minus_sign);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_int_literal_negative)
{
    parser_set_source(fix->parser, "test", "-9876543");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_int_lit_t* lit = (ast_int_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_INT_LIT, AST_KIND(lit));
    ASSERT_EQ(9876543, lit->value.magnitude);
    ASSERT_TRUE(lit->has_minus_sign);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_int_literal_with_suffix)
{
    parser_set_source(fix->parser, "test", "580i16");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_int_lit_t* lit = (ast_int_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_INT_LIT, AST_KIND(lit));
    ASSERT_EQ(580, lit->value.magnitude);
    ASSERT_FALSE(lit->has_minus_sign);
    ASSERT_EQ("i16", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_int_literal_with_separated_suffix)
{
    parser_set_source(fix->parser, "test", "-42_i8");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_int_lit_t* lit = (ast_int_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_INT_LIT, AST_KIND(lit));
    ASSERT_EQ(42, lit->value.magnitude);
    ASSERT_TRUE(lit->has_minus_sign);
    ASSERT_EQ("i8", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_int_literal_with_underscore)
{
    parser_set_source(fix->parser, "test", "1_234_567");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_int_lit_t* lit = (ast_int_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_INT_LIT, AST_KIND(lit));
    ASSERT_EQ(1234567, lit->value.magnitude);
    ASSERT_FALSE(lit->has_minus_sign);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_int_literal_overflow)
{
    // 18446744073709551616 is UINT64_MAX + 1, too large for u64
    parser_set_source(fix->parser, "test", "18446744073709551616");
    ast_expr_t* expr = parser_parse_expr(fix->parser);

    // Parser should create the node but report an error
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(1, vec_size(parser_errors(fix->parser)));

    // Check error message mentions overflow/too large
    compiler_error_t* error = vec_get(parser_errors(fix->parser), 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "too large"));

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_float)
{
    parser_set_source(fix->parser, "test", "42.5");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_float_lit_t* lit = (ast_float_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_FLOAT_LIT, AST_KIND(lit));
    ASSERT_EQ(42.5, lit->value);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_float_underscore)
{
    parser_set_source(fix->parser, "test", "42.500_000");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_float_lit_t* lit = (ast_float_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_FLOAT_LIT, AST_KIND(lit));
    ASSERT_EQ(42.500000, lit->value);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_float_suffix)
{
    parser_set_source(fix->parser, "test", "0.5f32");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_float_lit_t* lit = (ast_float_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_FLOAT_LIT, AST_KIND(lit));
    ASSERT_EQ(0.5, lit->value);
    ASSERT_EQ("f32", lit->suffix);

    ast_node_destroy(expr);
}

TEST(parser_literals_fixture_t, parse_float_exponent_notation)
{
    parser_set_source(fix->parser, "test", "1e-3");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_float_lit_t* lit = (ast_float_lit_t*)expr;
    ASSERT_EQ(AST_EXPR_FLOAT_LIT, AST_KIND(lit));
    ASSERT_EQ(1e-3, lit->value);
    ASSERT_EQ("", lit->suffix);

    ast_node_destroy(expr);
}
