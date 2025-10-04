#include "parser.h"

#include "ast/decl/param_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/printer.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "lexer.h"
#include "test_runner.h"

#include <stdarg.h>

TEST_FIXTURE(ut_parser_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(ut_parser_fixture_t)
{
    fix->parser = parser_create(lexer_create(nullptr));
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(ut_parser_fixture_t)
{
    parser_destroy(fix->parser);
}

#define ASSERT_TREES_EQUAL(a, b) \
    { \
        ast_printer_t* printer = ast_printer_create(); \
        char* printed_root = ast_printer_print_ast(printer, AST_NODE(a)); \
        char* printed_expected_tree = ast_printer_print_ast(printer, AST_NODE(b)); \
        ASSERT_EQ(printed_root, printed_expected_tree); \
        free(printed_root); \
        free(printed_expected_tree); \
        ast_printer_destroy(printer); \
    }

TEST(ut_parser_fixture_t, parse_basic_main_function)
{
    // Parse source code
    lexer_set_source(fix->parser->lexer, "int main() { return 0; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(root, nullptr);
    // TODO: Assert fix->parser has no errors

    // Construct an expected tree
    ast_root_t* expected_tree = ast_root_create_va(
        ast_fn_def_create_va("main",
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(0)),
                nullptr), nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(root, expected_tree);
}

TEST(ut_parser_fixture_t, parse_fn_parameters_and_calls_with_args)
{
    // Parse source code
    lexer_set_source(fix->parser->lexer, "int fn2(int arg1, int arg2) { return arg2; } "
        "int fn(int arg) { fn2(arg, arg); fn2(arg, 5); }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(root, nullptr);
    // TODO: Assert fix->parser has no errors

    // Construct an expected tree
    ast_root_t* expected_tree = ast_root_create_va(
        ast_fn_def_create_va("fn2",
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_ref_expr_create("arg2")),
                nullptr),
            ast_param_decl_create("int", "arg1"),
            ast_param_decl_create("int", "arg2"),
            nullptr),
        ast_fn_def_create_va("fn",
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("fn2"),
                        ast_ref_expr_create("arg"),
                        ast_ref_expr_create("arg"),
                        nullptr)),
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("fn2"),
                        ast_ref_expr_create("arg"),
                        ast_int_lit_create(5),
                        nullptr)),
                nullptr),
            ast_param_decl_create("int", "arg"),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(root, expected_tree);
}
