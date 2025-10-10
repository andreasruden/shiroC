#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/util/printer.h"
#include "compiler_error.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "test_runner.h"

#include <stdarg.h>

TEST_FIXTURE(ut_parser_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(ut_parser_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(ut_parser_fixture_t)
{
    parser_destroy(fix->parser);
}

#define ASSERT_TREES_EQUAL(a, b) \
    { \
        ast_printer_t* printer = ast_printer_create(); \
        char* printed_expected_tree = ast_printer_print_ast(printer, AST_NODE(a)); \
        char* printed_root = ast_printer_print_ast(printer, AST_NODE(b)); \
        ASSERT_EQ(printed_expected_tree, printed_root); \
        free(printed_root); \
        free(printed_expected_tree); \
        ast_printer_destroy(printer); \
    }

TEST(ut_parser_fixture_t, parse_basic_main_function)
{
    // Parse source code
    parser_set_source(fix->parser, "test", "fn main() -> i32 { return 0; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("main", ast_type_from_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(0)),
                nullptr), nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_fn_parameters_and_calls_with_args)
{
    // Parse source code
    parser_set_source(fix->parser, "test", "fn foo2(arg1: i32, arg2: i32) -> i32 { return arg2; } "
        "fn foo(arg: i32) { foo2(arg, arg); foo2(arg, 5); }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo2", ast_type_from_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_ref_expr_create("arg2")),
                nullptr),
            ast_param_decl_create("arg1", ast_type_from_builtin(TYPE_I32)),
            ast_param_decl_create("arg2", ast_type_from_builtin(TYPE_I32)),
            nullptr),
        ast_fn_def_create_va("foo", nullptr,
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("foo2"),
                        ast_ref_expr_create("arg"),
                        ast_ref_expr_create("arg"),
                        nullptr)),
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("foo2"),
                        ast_ref_expr_create("arg"),
                        ast_int_lit_create(5),
                        nullptr)),
                nullptr),
            ast_param_decl_create("arg", ast_type_from_builtin(TYPE_I32)),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, full_parse_with_simple_syntax_error)
{
    parser_set_source(fix->parser, "test", "fn foo() -> f32\n{ return 0 }\nfn foo2() -> i32\n{ return 10; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Verify the error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_EQ(1, vec_size(errors));
    compiler_error_t* err = vec_get(errors, 0);
    ASSERT_NEQ(nullptr, err);
    ASSERT_EQ("test", err->source_file);
    ASSERT_EQ(2, err->line);
    ASSERT_EQ(11, err->column);
    ASSERT_EQ("expected ';'", err->description);
    ASSERT_EQ(nullptr, err->offender);

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo", ast_type_from_builtin(TYPE_F32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(0)),
                nullptr),
            nullptr),
        ast_fn_def_create_va("foo2", ast_type_from_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(10)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, partial_parse_with_structural_error)
{
    parser_set_source(fix->parser, "test",
        "fn foo() -> i32\n"
        "{ return 0; }\n"
        "fn foo2() -> i32\n"
        "return 10; }\n"
        "fn foo3() -> i32\n"
        "{ return 20; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Verify the error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_EQ(1, vec_size(errors));
    compiler_error_t* err = vec_get(errors, 0);
    ASSERT_NEQ(nullptr, err);
    ASSERT_EQ("test", err->source_file);
    ASSERT_EQ(3, err->line);
    ASSERT_EQ(17, err->column);
    ASSERT_EQ("expected '{'", err->description);
    ASSERT_EQ(nullptr, err->offender);

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo", ast_type_from_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(0)),
                nullptr),
            nullptr),
        ast_fn_def_create_va("foo3", ast_type_from_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(20)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_basic_bin_op)
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

TEST(ut_parser_fixture_t, parse_left_associative_same_precedence)
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

TEST(ut_parser_fixture_t, parse_precedence_mul_before_add)
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

TEST(ut_parser_fixture_t, parse_precedence_add_then_mul)
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

TEST(ut_parser_fixture_t, parse_multiple_precedence_levels)
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

TEST(ut_parser_fixture_t, parse_parentheses_override_precedence)
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

TEST(ut_parser_fixture_t, parse_nested_parentheses)
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

TEST(ut_parser_fixture_t, parse_comparison_with_arithmetic)
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

TEST(ut_parser_fixture_t, parse_complex_mixed_precedence)
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

TEST(ut_parser_fixture_t, parse_chained_division)
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

TEST(ut_parser_fixture_t, parse_mixed_add_subtract)
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

TEST(ut_parser_fixture_t, parse_decl_stmt_no_init)
{
    parser_set_source(fix->parser, "test", "var x: i32;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_decl_stmt_with_init)
{
    parser_set_source(fix->parser, "test", "var x = 42;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", nullptr, ast_int_lit_create(42)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_var_decls_with_no_type)
{
    parser_set_source(fix->parser, "test", "var x;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(1, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(ast_var_decl_create("x", nullptr, nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_var_decls_force_type_inference)
{
    parser_set_source(fix->parser, "test", "var x: i32 = 42;");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(1, vec_size(parser_errors(fix->parser)));

    ast_stmt_t* expected = ast_decl_stmt_create(
        ast_var_decl_create("x", ast_type_from_builtin(TYPE_I32), ast_int_lit_create(42)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(ut_parser_fixture_t, parse_and_verify_source_locations)
{
    parser_set_source(fix->parser, "test",
        "fn main(argc: i32) {\n"
        "  var some_val = 23 * 10;\n"
        "  foo(some_val);\n"
        "}");

    ast_root_t* snippet = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, snippet);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_printer_t* printer = ast_printer_create(); \
    ast_printer_set_show_source_loc(printer, true);
    char* printed_snippet = ast_printer_print_ast(printer, AST_NODE(snippet));
    const char* expected_code =
        "Root\n"
        "  FnDef 'main' <test:1:1, test:4:2>\n"
        "    ParamDecl 'argc' 'i32' <test:1:9, test:1:18>\n"
        "    CompoundStmt <test:1:20, test:4:2>\n"
        "      DeclStmt <test:2:3, test:2:26>\n"
        "        VarDecl 'some_val' <test:2:3, test:2:25>\n"
        "          BinOp '*' <test:2:18, test:2:25>\n"
        "            IntLit '23' <test:2:18, test:2:20>\n"
        "            IntLit '10' <test:2:23, test:2:25>\n"
        "      ExprStmt <test:3:3, test:3:17>\n"
        "        CallExpr <test:3:3, test:3:16>\n"
        "          RefExpr 'foo' <test:3:3, test:3:6>\n"
        "          RefExpr 'some_val' <test:3:7, test:3:15>\n";
    ASSERT_EQ(expected_code, printed_snippet);
    ast_node_destroy(snippet);
    ast_printer_destroy(printer);
    free(printed_snippet);
}

TEST(ut_parser_fixture_t, parse_if_stmt_with_else)
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
            ast_int_lit_create(5)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("y", nullptr, ast_int_lit_create(10))),
            nullptr),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("z", nullptr, ast_int_lit_create(20))),
            nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(ut_parser_fixture_t, parse_if_stmt_no_else)
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
                ast_var_decl_create("x", nullptr, ast_int_lit_create(1))),
            nullptr),
        nullptr);  // No else branch

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(ut_parser_fixture_t, parse_if_stmt_else_if_chain)
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
            ast_int_lit_create(5)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("b", nullptr, ast_int_lit_create(2))),
            nullptr),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("c", nullptr, ast_int_lit_create(3))),
            nullptr));

    // Build outer if
    ast_stmt_t* expected = ast_if_stmt_create(
        ast_bin_op_create(TOKEN_GT,
            ast_ref_expr_create("x"),
            ast_int_lit_create(10)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(
                ast_var_decl_create("a", nullptr, ast_int_lit_create(1))),
            nullptr),
        inner_if);  // else branch is another if statement

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(ut_parser_fixture_t, parse_simple_assignment)
{
    parser_set_source(fix->parser, "test", "x = 5");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_expr_t* expected = ast_bin_op_create(TOKEN_ASSIGN, ast_ref_expr_create("x"), ast_int_lit_create(5));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(ut_parser_fixture_t, parse_chained_assignment)
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
            ast_int_lit_create(10)));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expected);
    ast_node_destroy(expr);
}

TEST(ut_parser_fixture_t, parse_assignment_with_expression)
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

TEST(ut_parser_fixture_t, parse_while_stmt_simple)
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
                ast_var_decl_create("y", nullptr, ast_int_lit_create(5))),
            nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(expected);
    ast_node_destroy(stmt);
}

TEST(ut_parser_fixture_t, parse_while_stmt_syntax_errors_but_valid_ast)
{
    parser_set_source(fix->parser, "test",
        "while i > 5\n"
        "  call_fn();");

    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);

    // Should produce a valid AST despite syntax errors
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(AST_STMT_WHILE, AST_KIND(stmt));
    ASSERT_LT(2, vec_size(parser_errors(fix->parser)));

    // Verify AST structure is intact
    ast_while_stmt_t* while_stmt = (ast_while_stmt_t*)stmt;

    // Condition should be parsed (i > 5)
    ASSERT_NEQ(nullptr, while_stmt->condition);
    ASSERT_EQ(AST_EXPR_BIN_OP, AST_KIND(while_stmt->condition));

    // Body should exist
    ASSERT_NEQ(nullptr, while_stmt->body);
    ASSERT_EQ(AST_STMT_EXPR, AST_KIND(while_stmt->body));
    ASSERT_EQ(AST_EXPR_CALL, AST_KIND(((ast_expr_stmt_t*)while_stmt->body)->expr));

    ast_node_destroy(stmt);
}
