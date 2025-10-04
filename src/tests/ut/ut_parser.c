#include "lexer.h"
#include "test_runner.h"

#include "parser.h"

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
    lexer_set_source(fix->parser->lexer, "int main() { return 0; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(root, nullptr);
    // TODO: Assert fix->parser has no warnings/errors

    // TODO: Compare ast_printer output to expected
    int v = 20;
    ASSERT_NEQ(20, v);
}
