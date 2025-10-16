#include "ast/def/fn_def.h"
#include "ast/expr/int_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "ast/util/printer.h"
#include "compiler_error.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_errors_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_errors_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_errors_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_errors_fixture_t, partial_parse_with_structural_error)
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
    ASSERT_LE(1, vec_size(errors));
    compiler_error_t* err = vec_get(errors, 0);
    ASSERT_NEQ(nullptr, err);
    ASSERT_EQ("test", err->source_file);
    ASSERT_EQ(3, err->line);
    ASSERT_EQ(17, err->column);
    ASSERT_EQ("expected '{'", err->description);
    ASSERT_EQ(nullptr, err->offender);

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_val(0)),
                nullptr),
            nullptr),
        ast_fn_def_create_va("foo3", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_val(20)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_errors_fixture_t, parse_and_verify_source_locations)
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
