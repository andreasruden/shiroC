#include "ast/decl/var_decl.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/inc_dec_stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/type.h"
#include "compiler_error.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_statements_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_statements_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_statements_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_statements_fixture_t, parse_decl_stmt_no_init)
{
    parser_set_source(fix->parser, "test", "var x: i32");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", ast_type_builtin(TYPE_I32), nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_statements_fixture_t, parse_decl_stmt_with_init)
{
    parser_set_source(fix->parser, "test", "var x = 42");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", nullptr, ast_int_lit_val(42)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_statements_fixture_t, parse_var_decls_with_no_type)
{
    parser_set_source(fix->parser, "test", "var x");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(1, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(ast_var_decl_create("x", nullptr, nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_statements_fixture_t, parse_var_decls_type_and_init_expr)
{
    parser_set_source(fix->parser, "test", "var x: i32 = 42");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", ast_type_builtin(TYPE_I32), ast_int_lit_val(42)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_statements_fixture_t, parse_if_stmt_with_else)
{
    parser_set_source(fix->parser, "test",
        "if (x > 5) {\n"
        "    var y = 10;\n"
        "} else {\n"
        "    var z = 20;\n"
        "}");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_if_stmt_create(
        ast_bin_op_create(TOKEN_GT,
            ast_ref_expr_create("x"),
            ast_int_lit_val(5)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("y", nullptr, ast_int_lit_val(10))),
            nullptr),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("z", nullptr, ast_int_lit_val(20))),
            nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, parse_if_stmt_no_else)
{
    parser_set_source(fix->parser, "test",
        "if (flag) {\n"
        "    var x = 1;\n"
        "}");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_if_stmt_create(
        ast_ref_expr_create("flag"),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("x", nullptr, ast_int_lit_val(1))),
            nullptr),
        nullptr);  // No else branch

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, parse_if_stmt_else_if_chain)
{
    parser_set_source(fix->parser, "test",
        "if (x > 10) {\n"
        "    var a = 1;\n"
        "} else if (x > 5) {\n"
        "    var b = 2;\n"
        "} else {\n"
        "    var c = 3;\n"
        "}");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Build inner if (the "else if" part)
    ast_stmt_t* inner_if = ast_if_stmt_create(
        ast_bin_op_create(TOKEN_GT,
            ast_ref_expr_create("x"),
            ast_int_lit_val(5)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("b", nullptr, ast_int_lit_val(2))),
            nullptr),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("c", nullptr, ast_int_lit_val(3))),
            nullptr));

    // Build outer if
    ast_stmt_t* expected = ast_if_stmt_create(
        ast_bin_op_create(TOKEN_GT,
            ast_ref_expr_create("x"),
            ast_int_lit_val(10)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("a", nullptr, ast_int_lit_val(1))),
            nullptr),
        inner_if);  // else branch is another if statement

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, parse_while_stmt_simple)
{
    parser_set_source(fix->parser, "test",
        "while (flag) {\n"
        "    var y = 5;\n"
        "}");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_while_stmt_create(
        ast_ref_expr_create("flag"),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("y", nullptr, ast_int_lit_val(5))),
            nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, parse_while_stmt_syntax_errors_but_valid_ast)
{
    parser_set_source(fix->parser, "test",
        "while i > 5 {\n"
        "  call_fn(); }");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);

    // Should produce a valid AST despite syntax errors
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(AST_STMT_WHILE, AST_KIND(stmt));
    ASSERT_LT(0, vec_size(parser_errors(fix->parser)));

    // Verify AST structure is intact
    ast_while_stmt_t* while_stmt = (ast_while_stmt_t*)stmt;

    // Condition should be parsed (i > 5)
    ASSERT_NEQ(nullptr, while_stmt->condition);
    ASSERT_EQ(AST_EXPR_BIN_OP, AST_KIND(while_stmt->condition));

    // Body should exist
    ASSERT_NEQ(nullptr, while_stmt->body);
    ASSERT_EQ(AST_STMT_COMPOUND, AST_KIND(while_stmt->body));

    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, invalid_int_literal)
{
    parser_set_source(fix->parser, "test", "var i = 08;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_LT(0, vec_size(parser_errors(fix->parser)));
    compiler_error_t* error = vec_get(&fix->parser->lex_errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "invalid integer literal"));

    ast_node_destroy(stmt);
}

TEST(parser_statements_fixture_t, parse_increment_stmt)
{
    parser_set_source(fix->parser, "test", "++i");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_inc_dec_stmt_create(ast_ref_expr_create("i"), true);

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_statements_fixture_t, parse_decrement_stmt)
{
    parser_set_source(fix->parser, "test", "--count");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_inc_dec_stmt_create(ast_ref_expr_create("count"), false);

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}
