#include "ast/decl/var_decl.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/null_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/type.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_pointers_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_pointers_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_pointers_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_pointers_fixture_t, parse_compound_stmt_with_address_of)
{
    parser_set_source(fix->parser, "test",
        "{ var i = 40;\n"
        "var ptr = &i; }");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(40))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr,
            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i")))),
        nullptr);

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_pointers_fixture_t, parse_dereference_of_address_of)
{
    parser_set_source(fix->parser, "test", "*&x");

    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_expr_t* expected = ast_unary_op_create(TOKEN_STAR, ast_unary_op_create(TOKEN_AMPERSAND,
        ast_ref_expr_create("x")));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(parser_pointers_fixture_t, parse_var_decl_with_pointer_to_pointer_type)
{
    parser_set_source(fix->parser, "test", "var ptr_to_ptr: i32**;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("ptr_to_ptr",
            ast_type_pointer(ast_type_pointer(ast_type_builtin(TYPE_I32))),
            nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_pointers_fixture_t, type_annotation_valid_when_init_expr_is_null_lit)
{
    parser_set_source(fix->parser, "test", "var ptr: i32* = null;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("ptr", ast_type_pointer(ast_type_builtin(TYPE_I32)),
            ast_null_lit_create()));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}
