#include "parser.h"

#include "ast/def/fn_def.h"
#include "ast/expr/int_lit.h"
#include "ast/printer.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "common/containers/ptr_vec.h"
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

TEST(ut_parser_fixture_t, parse_basic_main_function)
{
    // Parse source code
    lexer_set_source(fix->parser->lexer, "int main() { return 0; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(root, nullptr);
    // TODO: Assert fix->parser has no warnings/errors

    // Construct an expected tree
    ast_root_t* expected_tree = ast_root_create_va(
        ast_fn_def_create("main",
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_create(0)),
                nullptr)),
        nullptr);

    // Assert that parse tree and expected tree are equal
    ast_printer_t* printer = ast_printer_create();
    char* printed_root = ast_printer_print_ast(printer, AST_NODE(root));
    char* printed_expected_tree = ast_printer_print_ast(printer, AST_NODE(expected_tree));
    ASSERT_EQ(printed_root, printed_expected_tree);
    free(printed_root);
    free(printed_expected_tree);
}
