#include "ast/def/import_def.h"
#include "ast/node.h"
#include "ast/root.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_misc_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_misc_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_misc_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_misc_fixture_t, parse_two_import_definitions)
{
    // Parse source code with 2 import statements
    parser_set_source(fix->parser, "test", "import Project.Module;\nimport Std.IO;");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_import_def_create("Project", "Module"),
        ast_import_def_create("Std", "IO"),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_misc_fixture_t, error_import_after_function)
{
    // Import statement appearing after function definition should error
    parser_set_source(fix->parser, "test", "fn main() {}\nimport Std.IO;");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Should have an error
    ASSERT_NEQ(0, vec_size(parser_errors(fix->parser)));
    compiler_error_t* err = vec_get(parser_errors(fix->parser), 0);
    ASSERT_NEQ(nullptr, strstr(err->description, "import"));

    ast_node_destroy(root);
}
